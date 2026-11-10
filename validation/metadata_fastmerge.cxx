// Isolated checksum metadata regression research. No ROOT source modifications.
#include <ROOT/RNTupleWriter.hxx>
#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RNTupleMerger.hxx>
#include <ROOT/RNTupleWriteOptions.hxx>
#include <ROOT/RPageStorageFile.hxx>
#include <ROOT/RNTupleReadOptions.hxx>
#include <ROOT/RNTupleDescriptor.hxx>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
namespace RI = ROOT::Internal;
namespace RE = ROOT::Experimental::Internal;
void write(const std::string &path, int offset, bool checksums) {
   auto model = ROOT::RNTupleModel::Create();
   auto value = model->MakeField<int>("value");
   ROOT::RNTupleWriteOptions options;
   options.SetCompression(505);
   options.SetEnablePageChecksums(checksums);
   auto writer = ROOT::RNTupleWriter::Recreate(std::move(model), "ntuple", path, options);
   for (int i = 0; i < 130; ++i) {
      *value = offset + i;
      writer->Fill();
      if (i % 31 == 0) writer->CommitCluster();
   }
}
struct Counts { unsigned on = 0, off = 0; };
Counts metadata(const std::string &path) {
   auto source = RI::RPageSource::Create("ntuple", path, ROOT::RNTupleReadOptions());
   source->Attach();
   const auto desc = source->GetSharedDescriptorGuard();
   Counts counts;
   for (const auto &cluster : desc->GetClusterIterable()) {
      std::unordered_set<ROOT::DescriptorId_t> seen;
      for (const auto &col : desc->GetColumnIterable()) {
         const auto id = col.GetPhysicalId();
         if (!seen.insert(id).second || !cluster.ContainsColumn(id)) continue;
         for (const auto &page : cluster.GetPageRange(id).GetPageInfos())
            page.HasChecksum() ? ++counts.on : ++counts.off;
      }
   }
   if (counts.on + counts.off == 0) throw std::runtime_error("No page metadata found");
   std::cout << "METADATA " << path << " ON=" << counts.on << " OFF=" << counts.off << "\n";
   return counts;
}
int main() {
   try {
      write("off_a.root", 0, false);
      write("off_b.root", 1000, false);
      auto a = metadata("off_a.root");
      auto b = metadata("off_b.root");
      ROOT::RNTupleWriteOptions dstOpts;
      dstOpts.SetCompression(505);
      dstOpts.SetEnablePageChecksums(true);
      dstOpts.SetEnableSamePageMerging(false); // Isolates the fast-copy metadata contract from the H2 assertion.
      std::vector<std::unique_ptr<RI::RPageSource>> sources;
      sources.push_back(RI::RPageSource::Create("ntuple","off_a.root"));
      sources.push_back(RI::RPageSource::Create("ntuple","off_b.root"));
      std::vector<RI::RPageSource*> ptrs;
      for (auto &source : sources) ptrs.push_back(source.get());
      {
         auto dest = std::make_unique<RI::RPageSinkFile>("ntuple", "merged.root", dstOpts);
         RE::RNTupleMerger merger(std::move(dest));
         auto result = merger.Merge(ptrs);
         if (!result) throw std::runtime_error(result.GetError()->GetReport());
      }
      auto reader = ROOT::RNTupleReader::Open("ntuple", "merged.root");
      if (reader->GetNEntries() != 260) throw std::runtime_error("Wrong entry count");
      auto view = reader->GetView<int>("value");
      for (unsigned i=0;i<260;++i) {
         const int expected = i < 130 ? i : 1000 + int(i)-130;
         if (view(i) != expected) throw std::runtime_error("Value mismatch");
      }
      std::cout << "VALUES_PASS=260/260\n";
      auto out = metadata("merged.root");
      if (a.on != 0 || b.on != 0 || a.off == 0 || b.off == 0)
         throw std::runtime_error("Invalid source control");
      std::cout << "DESTINATION_CHECKSUM_OPTION=true OUTPUT_PAGES_ON=" << out.on
                << " OUTPUT_PAGES_OFF=" << out.off << "\n";
      return 0;
   } catch (const std::exception &ex) {
      std::cerr << "FAILED: " << ex.what() << "\n";
      return 1;
   }
}
