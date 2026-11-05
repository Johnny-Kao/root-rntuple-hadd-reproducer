// Minimal public CLI reproduction helper. No ROOT implementation changes.
#include <ROOT/RNTupleWriter.hxx>
#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleWriteOptions.hxx>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
using namespace ROOT;
static void generate(const char *path, int src, bool checksums, unsigned compression) {
  auto model=RNTupleModel::Create();
  auto id=model->MakeField<std::int32_t>("id");
  auto value=model->MakeField<double>("value");
  RNTupleWriteOptions opts;
  opts.SetEnablePageChecksums(checksums);
  opts.SetCompression(compression);
  auto writer=RNTupleWriter::Recreate(std::move(model),"ntuple",path,opts);
  for(int i=0;i<130;++i) {
    *id=src*10000+i;
    *value=src*1000.0+i*1.25-17.5;
    writer->Fill();
    if(i%31==0) writer->CommitCluster();
  }
  std::cout<<"GENERATED "<<path<<" source="<<src<<" checksum="<<checksums<<" compression="<<compression<<"\n";
}
static int verify(const char *path) {
  auto reader=RNTupleReader::Open("ntuple",path);
  if(reader->GetNEntries()!=260) {std::cerr<<"ENTRIES_BAD "<<reader->GetNEntries()<<"\n";return 1;}
  auto id=reader->GetView<std::int32_t>("id");
  auto value=reader->GetView<double>("value");
  for(int i=0;i<260;++i){
    const int src=i<130?1:2, n=i%130;
    const int expId=src*10000+n;
    const double expValue=src*1000.0+n*1.25-17.5;
    if(id(i)!=expId||value(i)!=expValue){
      std::cerr<<"SILENT_DATA_ERROR row="<<i<<" id="<<id(i)<<" value="<<value(i)<<"\n";
      return 2;
    }
  }
  std::cout<<"PRODUCTION_HADD_READBACK_PASS 260/260"<<std::endl;
  return 0;
}
int main(int argc,char**argv){
 try{
  if(argc==2)return verify(argv[1]);
  if(argc==6 && std::string(argv[1])=="generate"){
    generate(argv[2],std::stoi(argv[3]),std::stoi(argv[4])!=0,std::stoul(argv[5]));return 0;
  }
  return 64;
 }catch(const std::exception &e){std::cerr<<"HARNESS_ERROR: "<<e.what()<<std::endl;return 1;}
}
