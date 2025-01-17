#include <limits>
#include <algorithm>
#include <cctype>
#include <utility>
#include <memory>

#include "TMVA/RModel.hxx"
#include "TMVA/SOFIE_common.hxx"

#include "TFile.h"

namespace TMVA{
namespace Experimental{
namespace SOFIE{

   std::underlying_type_t<Options> operator|(Options opA, Options opB) {
      return static_cast<std::underlying_type_t<Options>>(opA) | static_cast<std::underlying_type_t<Options>>(opB);
   }
   std::underlying_type_t<Options> operator|(std::underlying_type_t<Options> opA, Options opB) {
      return opA | static_cast<std::underlying_type_t<Options>>(opB);
   }


   RModel::RModel(RModel&& other){
      fInputTensorInfos = std::move(other.fInputTensorInfos);
      fReadyInputTensorInfos = std::move(other.fReadyInputTensorInfos);
      fOutputTensorNames = other.fOutputTensorNames;
      fInputTensorNames = other.fInputTensorNames;
      fOperators = std::move(other.fOperators);
      fInitializedTensors = std::move(other.fInitializedTensors);
      fIntermediateTensorInfos = std::move(other.fIntermediateTensorInfos);
      fName = other.fName;
      fFileName = other.fFileName;
      fParseTime = other.fParseTime;
      fGC = other.fGC;
      fNeededBlasRoutines = other.fNeededBlasRoutines;
      fNeededStdLib = other.fNeededStdLib;
   }

   RModel& RModel::operator=(RModel&& other){
      fInputTensorInfos = std::move(other.fInputTensorInfos);
      fReadyInputTensorInfos = std::move(other.fReadyInputTensorInfos);
      fOutputTensorNames = other.fOutputTensorNames;
      fInputTensorNames = other.fInputTensorNames;
      fOperators = std::move(other.fOperators);
      fInitializedTensors = std::move(other.fInitializedTensors);
      fIntermediateTensorInfos = std::move(other.fIntermediateTensorInfos);
      fName = other.fName;
      fFileName = other.fFileName;
      fParseTime = other.fParseTime;
      fGC = other.fGC;
      fNeededBlasRoutines = other.fNeededBlasRoutines;
      fNeededStdLib = other.fNeededStdLib;
      return *this;
   }

   RModel::RModel(std::string name, std::string parsedtime): fFileName (name), fParseTime(parsedtime) {
      fName = fFileName.substr(0, fFileName.rfind("."));
   }

   const std::vector<size_t>& RModel::GetTensorShape(std::string name){
      auto f = fReadyInputTensorInfos.find(name);
      if (f != fReadyInputTensorInfos.end()){
         return f->second.shape;
      }
      auto f2 = fInitializedTensors.find(name);
      if (f2 != fInitializedTensors.end()){
         return f2->second.fShape;
      }
      auto f3 = fInputTensorInfos.find(name);
      if (f3 != fInputTensorInfos.end()){
         throw std::runtime_error("TMVA SOFIE tensor [" + name + "] is an input tensor with unspecified dimension parameter");
      }
      auto f4 = fIntermediateTensorInfos.find(name);
      if (f4 != fIntermediateTensorInfos.end()){
         return f4->second.shape;
      }

      throw std::runtime_error("TMVA SOFIE tensor [" + name + "] for which the shape is requested is not found");
   }

   const ETensorType& RModel::GetTensorType(std::string name){
      auto f = fReadyInputTensorInfos.find(name);
      if (f != fReadyInputTensorInfos.end()){
         return f->second.type;
      }
      auto f2 = fInitializedTensors.find(name);
      if (f2 != fInitializedTensors.end()){
         return f2->second.fType;
      }
      auto f3 = fInputTensorInfos.find(name);
      if (f3 != fInputTensorInfos.end()){
         return f3->second.type;
      }
      auto f4 = fIntermediateTensorInfos.find(name);
      if (f4 != fIntermediateTensorInfos.end()){
         return f4->second.type;
      }

      throw std::runtime_error("TMVA SOFIE tensor [" + name + "] for which the type is requested is not found");
   }

   bool RModel::CheckIfTensorAlreadyExist(std::string tensor_name){
      if (fReadyInputTensorInfos.find(tensor_name) != fReadyInputTensorInfos.end())  return true;
      if (fInitializedTensors.find(tensor_name) != fInitializedTensors.end()) return true;
      if (fIntermediateTensorInfos.find(tensor_name) != fIntermediateTensorInfos.end()) return true;
      return false;
   }

   void RModel::AddInputTensorInfo(std::string input_name, ETensorType type, std::vector<Dim> shape){
      input_name = UTILITY::Clean_name(input_name);
      if (CheckIfTensorAlreadyExist(input_name)){
         throw std::runtime_error("TMVA-SOFIE: input tensor with name " + input_name + " already exists \n");
      }

      InputTensorInfo inputInfo { type, shape };
      fInputTensorInfos[input_name] = inputInfo;
   }

   void RModel::AddInputTensorInfo(std::string input_name, ETensorType type, std::vector<size_t> shape){
      input_name = UTILITY::Clean_name(input_name);
      if (CheckIfTensorAlreadyExist(input_name)){
         throw std::runtime_error("TMVA-SOFIE: input tensor with name " + input_name + " already exists \n");
      }
      TensorInfo inputInfo { type, shape };
      fReadyInputTensorInfos[input_name] = inputInfo;
   }

   void RModel::AddInputTensorName(std::string input_name) {
       fInputTensorNames.push_back(UTILITY::Clean_name(input_name));
   }

   void RModel::AddOperator(std::unique_ptr<ROperator> op, int order_execution){
      AddBlasRoutines(op->GetBlasRoutines());
      auto libs = op->GetStdLibs();
      for (auto& stdlib : libs) {
         AddNeededStdLib(stdlib);
      }
      if (order_execution >= 0) {
         fOperators.insert(fOperators.begin() + order_execution, std::move(op));
      }else{
         fOperators.push_back(std::move(op));
      }
   }

   void RModel::AddInitializedTensor(std::string tensor_name, ETensorType type, std::vector<std::size_t> shape, std::shared_ptr<void> data){
      tensor_name = UTILITY::Clean_name(tensor_name);
      //NB: own data
      if (CheckIfTensorAlreadyExist(tensor_name)){
         throw std::runtime_error("TMVA-SOFIE: initialized tensor with name " + tensor_name + " already exists \n");
      }
      InitializedTensor new_tensor {type, shape, data};
      fInitializedTensors[tensor_name] = new_tensor;

   }

   bool RModel::IsInitializedTensor(const std::string& tensorName) const {
      std::string name = UTILITY::Clean_name(tensorName);
      return fInitializedTensors.find(name) != fInitializedTensors.end();
   }

   void RModel::AddIntermediateTensor(std::string tensor_name, ETensorType type, std::vector<std::size_t> shape){
      tensor_name = UTILITY::Clean_name(tensor_name);
      if (CheckIfTensorAlreadyExist(tensor_name)){
         throw std::runtime_error("TMVA-SOFIE: intermediate tensor with name " + tensor_name + " already exists \n");
      }
      TensorInfo new_tensor {type, shape};
      fIntermediateTensorInfos[tensor_name] = new_tensor;
   }

   void RModel::AddOutputTensorNameList(std::vector<std::string> outputtensornames){
      for(auto& it : outputtensornames){
         fOutputTensorNames.push_back(UTILITY::Clean_name(it));
      }
   }

   void RModel::UpdateOutputTensorList(std::vector<std::string> curr_output_tensors, std::vector<std::string> new_output_tensors){
      for(auto& it:curr_output_tensors){
         fOutputTensorNames.erase(std::remove(fOutputTensorNames.begin(), fOutputTensorNames.end(), it), fOutputTensorNames.end());
      }
      fOutputTensorNames.insert(fOutputTensorNames.end(), new_output_tensors.begin(), new_output_tensors.end());
   }

   void RModel::UpdateInitializedTensor(std::string tensor_name, ETensorType type, std::vector<std::size_t> shape, std::shared_ptr<void> data){
      tensor_name = UTILITY::Clean_name(tensor_name);
      if (!CheckIfTensorAlreadyExist(tensor_name)){
         throw std::runtime_error("TMVA-SOFIE: tensor " + tensor_name + " not found when trying to update it");
      }
      InitializedTensor new_tensor {type, shape, data};
      fInitializedTensors[tensor_name] = new_tensor;
   }

   std::shared_ptr<void> RModel::GetInitializedTensorData(std::string tensor_name){
      auto f = fInitializedTensors.find(tensor_name);
      if (f == fInitializedTensors.end()){
         throw std::runtime_error("TMVA-SOFIE: tensor " + tensor_name + " not found when trying to get its data");
      }else{
         return f->second.fData;
      }
   }

   void RModel::Initialize(int batchSize){
      // check if there are only parametrized input tensor and convert in
      // ready input tensor according to batch size
      // convert parametric shape to a dimensional shape
      fIntermediateTensorInfos.clear();
      if (fReadyInputTensorInfos.size() != fInputTensorNames.size()) {
         if ( fReadyInputTensorInfos.size() + fInputTensorInfos.size() != fInputTensorNames.size())
            throw std::runtime_error("TMVA-SOFIE: RModel::Initializes: invalid inputs");
         for (auto & input : fInputTensorInfos) {
            std::vector<size_t> shape;
            shape.reserve(input.second.shape.size());
            for (auto & d : input.second.shape){
               if (d.isParam)
                  shape.push_back(batchSize);
               else
                  shape.push_back(d.dim);
            }
            AddInputTensorInfo(input.first, input.second.type, shape);
         }
      }
      // check if there are initialized tensors to write in a weight file
      // support for the time being only wheight of FLOAT type
      if (fUseWeightFile) {
         bool modelHasWeights = false;
         for (auto& i: fInitializedTensors){
            if (i.second.fType == ETensorType::FLOAT) {
               modelHasWeights = true;
               break;
            }
         }
         if (!modelHasWeights) fUseWeightFile = false;
      }


      for (auto& i : fOperators){
         //std::cout << "initialize operator  " << typeid(*i).name() << std::endl;
         i->Initialize(*this);
      }
   }

   void RModel::Generate(std::underlying_type_t<Options> options, int batchSize) {
      // session flag is used in operator initialize
      if (static_cast<std::underlying_type_t<Options>>(Options::kNoSession) & options) {
         fUseSession = false;
         fWeightFile = WeightFileType::None;
      }
      if (static_cast<std::underlying_type_t<Options>>(Options::kNoWeightFile) & options) {
         fUseWeightFile = false;
         fWeightFile = WeightFileType::None;
      }
      if (static_cast<std::underlying_type_t<Options>>(Options::kRootBinaryWeightFile) & options) {
         fUseWeightFile = true;
         fWeightFile = WeightFileType::RootBinary;
      }
      if (fUseWeightFile && !fUseSession) {
         throw
            std::runtime_error("TMVA-SOFIE: RModel::Generate: cannot use a separate weight file without generating a Session class");
      }
      fGC.clear();
      Initialize(batchSize);
      fGC += ("//Code generated automatically by TMVA for Inference of Model file [" + fFileName + "] at [" + fParseTime.substr(0, fParseTime.length()-1) +"] \n");
      // add header guards
      std::string hgname = fName;
      std::transform(hgname.begin(), hgname.end(), hgname.begin(), [](unsigned char c){ return std::toupper(c);} );
      hgname = "ROOT_TMVA_SOFIE_" + hgname;
      fGC += "\n#ifndef " + hgname + "\n";
      fGC += "#define " + hgname + "\n\n";
      for (auto& i: fNeededStdLib) {
         fGC += "#include<" + i + ">\n";
      }
      for (auto& i: fCustomOpHeaders) {
         fGC += "#include \"" + i + "\"\n";
      }
      // for the session we need to include SOFIE_Common functions
      //needed for convolution operator (need to add a flag)
      fGC += "#include \"TMVA/SOFIE_common.hxx\"\n";
      if (fUseWeightFile)
         fGC += "#include <fstream>\n";
      // Include TFile when saving the weights in a binary ROOT file
      if (fWeightFile == WeightFileType::RootBinary)
         fGC += "#include \"TFile.h\"\n";

      fGC += "\nnamespace TMVA_SOFIE_" + fName + "{\n";
      if (!fNeededBlasRoutines.empty()) {
         fGC += ("namespace BLAS{\n");
         for (auto &routine : fNeededBlasRoutines) {
            if (routine == "Gemm") {
               fGC += ("\textern \"C\" void sgemm_(const char * transa, const char * transb, const int * m, const int * n, const int * k,\n"
                       "\t                       const float * alpha, const float * A, const int * lda, const float * B, const int * ldb,\n"
                       "\t                       const float * beta, float * C, const int * ldc);\n");
            } else if (routine == "Gemv") {
               fGC += ("\textern \"C\" void sgemv_(const char * trans, const int * m, const int * n, const float * alpha, const float * A,\n"
                       "\t                       const int * lda, const float * X, const int * incx, const float * beta, const float * Y, const int * incy);\n");
            } else if (routine == "Axpy") {
               fGC += ("\textern \"C\" void saxpy_(const int * n, const float * alpha, const float * x,\n"
                       "\t                         const int * incx, float * y, const int * incy);\n");
            } else if (routine == "Copy") {
               fGC += ("\textern \"C\" void scopy_(const int *n, const float* x, const int *incx, float* y, const int* incy);\n");
            }
         }
         fGC += ("}//BLAS\n");
      }
      if (fUseSession) {
         fGC += "struct Session {\n";
      }
      for (auto& i: fInitializedTensors){
         if (i.second.fType == ETensorType::FLOAT){
            size_t length = 1;
            for (auto & dim: i.second.fShape){
               length *= dim;
            }
            if (!fUseWeightFile) {
               fGC += "float tensor_" + i.first + "[" + std::to_string(length) + "] = {";
               std::shared_ptr<float> data = std::static_pointer_cast<float>(i.second.fData);
               std::stringstream floats;
               for (size_t idx = 0; idx < length-1; idx++){
                  floats << std::setprecision(std::numeric_limits<float>::max_digits10) << data.get()[idx] << ", ";
               }
               floats << std::setprecision(std::numeric_limits<float>::max_digits10) << data.get()[length-1];
               fGC += floats.str();
               fGC += "};\n";
            }
            else {
               fGC += "std::vector<float> fTensor_" + i.first + " = std::vector<float>(" + std::to_string(length) + ");\n";
               fGC += "float * tensor_" + i.first + " = fTensor_" + i.first + ".data();\n";
            }

         }
      }
      for (auto&i: fIntermediateTensorInfos){
         size_t length = ConvertShapeToLength(i.second.shape);
         if (i.second.type == ETensorType::FLOAT){
            fGC += "std::vector<float> fTensor_" + i.first  + " = std::vector<float>(" + std::to_string(length) + ");\n";
            fGC += "float * tensor_" + i.first + " = fTensor_" + i.first  + ".data();\n";
         }
         if (i.second.type == ETensorType::DOUBLE){
            fGC += "std::vector<double> fTensor_" + i.first  + " = std::vector<double>(" + std::to_string(length) + ");\n";
            fGC += "double * tensor_" + i.first + " = fTensor_" + i.first  + ".data();\n";
         }
         if (i.second.type == ETensorType::INT64){
            fGC += "std::vector<int64_t> fTensor_" + i.first  + " = std::vector<int64_t>(" + std::to_string(length) + ");\n";
            fGC += "int64_t * tensor_" + i.first + " = fTensor_" + i.first  + ".data();\n";
         }
      }
      if (fUseSession) {
         // add here specific operator code that needs to define session data members
         fGC += "\n";
         for (size_t id = 0; id < fOperators.size(); id++) {
            std::string opName = std::to_string(id);
            fGC += fOperators[id]->GenerateSessionMembersCode(opName);
         }
         fGC += "\n";
         // here add initialization and reading of weight tensors
         if (fUseWeightFile) {
            fGC += "Session(std::string filename =\"\") {\n";
            fGC += "   if (filename.empty()) filename = \"" + fName;
            if (fWeightFile == WeightFileType::Text) {
             fGC += ".dat\";\n";
            }
            if (fWeightFile == WeightFileType::RootBinary) {
               fGC += ".root\";\n";
            }
            ReadInitializedTensorsFromFile();
            //fUseWeightFile = fUseWeightFile;
         } else {
            // no need to pass weight file since it is not used
            // keep passing a string for compatibility
            fGC += "Session(std::string = \"\") {\n";
         }
         // add here initialization code
         for (size_t id = 0; id < fOperators.size() ; id++){
            fGC += fOperators[id]->GenerateInitCode();
         }
         fGC += "}\n\n";
      }

      size_t outputSize = fOutputTensorNames.size();
      // assume output types are all the same
      std::string outputType;
      if (outputSize == 1) {
         auto f = fIntermediateTensorInfos.find(fOutputTensorNames[0]);
         if (f == fIntermediateTensorInfos.end()){
            throw std::runtime_error("TMVA-SOFIE: output tensor " + fOutputTensorNames[0] + " not found when trying to get its info");
         }else{
            outputType = ConvertTypeToString(f->second.type);
            fGC += "std::vector<" + outputType + "> ";
         }
      } else {
         std::vector<ETensorType> outputTensorsTypes(outputSize);
         for (size_t i = 0; i < outputSize; i++) {
            auto f = fIntermediateTensorInfos.find(fOutputTensorNames[i]);
            if (f == fIntermediateTensorInfos.end()) {
               throw std::runtime_error("TMVA-SOFIE: output tensor " + fOutputTensorNames[i]
                  + " not found when trying to get its info");
            } else {
               outputTensorsTypes[i] = f->second.type;
            }
         }
         // assume all output types are the same
         outputType = ConvertTypeToString(outputTensorsTypes[0]);
         for (size_t i = 0; i < outputSize; i++) {
            if (outputTensorsTypes[i] != outputTensorsTypes[0]) {
               throw std::runtime_error("TMVA-SOFIE: output tensor " + fOutputTensorNames[i] + " is of different type.");
            }
         }
         fGC += "std::vector<std::vector<" + outputType + ">> ";
      }

      fGC += "infer(";
      for(size_t i = 0; i<fInputTensorNames.size(); ++i){
         switch((fReadyInputTensorInfos[fInputTensorNames[i]]).type){
            case  ETensorType::FLOAT :{
               fGC += "float* tensor_" + fInputTensorNames[i] + ",";
               break;
            }
            case  ETensorType::INT32 :{
               fGC += "int32_t* tensor_" + fInputTensorNames[i] + ",";
               break;
            }
            case  ETensorType::INT64 :{
               fGC += "int64_t* tensor_" + fInputTensorNames[i] + ",";
               break;
            }
            case  ETensorType::DOUBLE :{
               fGC += "double* tensor_" + fInputTensorNames[i] + ",";
               break;
            }
            default: {
               throw std::runtime_error("TMVA-SOFIE: input tensor " + fInputTensorNames[i] + " is of a data type which is not yet supported.");
            }
         }
      }
      fGC.pop_back(); //remove last ","
      fGC += "){\n";

      const std::string SP = "   ";

      for (size_t id = 0; id < fOperators.size() ; id++){
         fGC+= (fOperators[id]->Generate(std::to_string(id)));
      }
      if (outputSize == 1) {
         size_t outputLength = ConvertShapeToLength(GetTensorShape(fOutputTensorNames[0]));

         fGC += SP + "std::vector<" + outputType + "> ret (tensor_" + fOutputTensorNames[0] + ", tensor_" + fOutputTensorNames[0] + " + " +
               std::to_string(outputLength) + ");\n";
      } else {
         for (size_t i = 0; i < outputSize; i++) {
            if (!fOutputTensorNames[i].empty()) {
               size_t outputLength = ConvertShapeToLength(GetTensorShape(fOutputTensorNames[i]));
               fGC += SP + "std::vector<" + outputType + "> ret_";
               fGC += std::to_string(i);
               fGC += " (tensor_" + fOutputTensorNames[i] + ", tensor_" + fOutputTensorNames[i] + " + " +
               std::to_string(outputLength) + ");\n";
            }
         }
         fGC += SP + "std::vector<std::vector<" + outputType + ">> ret({";
         for (size_t i = 0; i < outputSize; i++) {
            if (fOutputTensorNames[i].empty()) {
               fGC += "{}";
            } else {
               fGC += "ret_";
               fGC += std::to_string(i);
            }
            if (i < outputSize - 1) {
               fGC += ",";
            }
         }
         fGC += "});\n";
      }
      fGC += SP + "return ret;\n";
      fGC += "}\n";
      if (fUseSession) {
         fGC += "};\n";
      }
      fGC += ("} //TMVA_SOFIE_" + fName + "\n");
      fGC += "\n#endif  // " + hgname + "\n";
      fGC_CPU = fGC;
   }

   void RModel::GenerateGPU(std::underlying_type_t<Options> options, int batchSize){
      if (static_cast<std::underlying_type_t<Options>>(Options::kNoSession) & options) {
         fUseSession = false;
         fWeightFile = WeightFileType::None;
      }
      if (static_cast<std::underlying_type_t<Options>>(Options::kNoWeightFile) & options) {
         fUseWeightFile = false;
         fWeightFile = WeightFileType::None;
      }
      if (static_cast<std::underlying_type_t<Options>>(Options::kRootBinaryWeightFile) & options) {
         fUseWeightFile = true;
         fWeightFile = WeightFileType::RootBinary;
      }
      if (fUseWeightFile && !fUseSession) {
         throw std::runtime_error("TMVA-SOFIE: RModel::Generate: cannot use a separate weight file without generating a Session class");
      }
      fGC.clear();
      Initialize(batchSize);

      fGC += ("//Code generated automatically by TMVA for Inference of Model file [" + fFileName + "] at [" + fParseTime.substr(0, fParseTime.length()-1) +"] \n");
      // add header guards
      std::string hgname = fName;
      std::transform(hgname.begin(), hgname.end(), hgname.begin(), [](unsigned char c){ return std::toupper(c);} );
      hgname = "TMVA_SOFIE_" + hgname;
      fGC += "\n#ifndef " + hgname + "\n";
      fGC += "#define " + hgname + "\n\n";
      for (auto& i: fNeededStdLib) {
         fGC += "#include<" + i + ">\n";
      }
      for (auto& i: fCustomOpHeaders) {
         fGC += "#include \"" + i + "\"\n";
      }

      // include SYCL libraries
      fGC += "#include \"CL/sycl.hpp\"\n";

      std::string gemm;
      std::string copy;
      std::string axpy;
      std::string transpose;
      std::string nontrans;
      std::string trans;
      std::string copy_batch;
      std::string scal;
      

      // include BLAS libraries, if needed
      if (gpu_blas == MKLBLAS) { 
         gemm = "oneapi::mkl::blas::gemm(q, ";
         copy = "oneapi::mkl::blas::copy(q, ";
         axpy = "oneapi::mkl::blas::axpy(q, ";
         transpose = "oneapi::mkl::transpose ";
         nontrans = "oneapi::mkl::transpose::nontrans";
         trans = "oneapi::mkl::transpose::trans";
         copy_batch = "oneapi::mkl::blas::copy_batch(q, ";
         scal = "oneapi::mkl::blas::scal(q, ";
      }
      else {
         gemm = "blas::_gemm(sb_handle, ";
         copy = "blas::_copy(sb_handle, ";
         axpy = "blas::_axpy(sb_handle, ";
         transpose = "char ";
         nontrans = "\'n\'";
         trans = "\'t\'";
         copy_batch = "";
         scal = "blas::_scal(sb_handle, ";
      }
      
      if (gpu_blas == MKLBLAS) {
         fGC += "#include \"oneapi/mkl/blas.hpp\"\n";
         fGC += "#include \"mkl.h\"\n";
      }
      else {
         fGC += "#include \"portblas.hpp\"\n";
      }

      // for the session we need to include SOFIE_Common functions
      //needed for convolution operator (need to add a flag)
      fGC += "#include \"TMVA/SOFIE_common.hxx\"\n";
      fGC += "#include \"TMVA/SOFIE_GPU_common.hxx\"\n";
      if (fUseWeightFile)
         fGC += "#include <fstream>\n";

      if (fWeightFile == WeightFileType::RootBinary)
         fGC += "#include \"TFile.h\"\n";

      fGC += "\nnamespace TMVA_SOFIE_" + fName + "{\n";
      if (fUseSession) {
         fGC += "struct Session {\n";
      }

      const std::string SP = "    "; //tab
      for (auto& i: fInitializedTensors){
         if (i.second.fType == ETensorType::FLOAT){
            size_t length = 1;
            for (auto & dim: i.second.fShape){
               length *= dim;
            }
            if (!fUseWeightFile) {
               fGC += SP + "float tensor_" + i.first + "[" + std::to_string(length) + "] = {";
               std::shared_ptr<float> data = std::static_pointer_cast<float>(i.second.fData);
               std::stringstream floats;
               for (size_t idx = 0; idx < length-1; idx++){
                  floats << std::setprecision(std::numeric_limits<float>::max_digits10) << data.get()[idx] << ", ";
               }
               floats << std::setprecision(std::numeric_limits<float>::max_digits10) << data.get()[length-1];
               fGC += floats.str();
               fGC += "};\n";
               fGC += SP + "std::vector<float> fTensor_" + i.first + "(tensor_" + i.first + ", ";
               fGC += "tensor_" + i.first + " + sizeof(tensor_" + i.first + ") / sizeof(";
               fGC += "tensor_" + i.first + "[0]));\n";
            }
            else {
               fGC += SP + "std::vector<float> fTensor_" + i.first + " = std::vector<float>(" + std::to_string(length) + ");\n";
               fGC += SP + "float * tensor_" + i.first + " = fTensor_" + i.first + ".data();\n";
            }
            fGC += "\n";
         }
      }

      for (auto&i: fIntermediateTensorInfos){
         size_t length = ConvertShapeToLength(i.second.shape);
         if (i.second.type == ETensorType::FLOAT){
            fGC += SP + "std::vector<float> fTensor_" + i.first  + " = std::vector<float>(" + std::to_string(length) + ");\n";
            fGC += SP + "float * tensor_" + i.first + " = fTensor_" + i.first  + ".data();\n";
         }
         if (i.second.type == ETensorType::DOUBLE){
            fGC += SP + "std::vector<double> fTensor_" + i.first  + " = std::vector<double>(" + std::to_string(length) + ");\n";
            fGC += SP + "double * tensor_" + i.first + " = fTensor_" + i.first  + ".data();\n";
         }
         if (i.second.type == ETensorType::INT64){
            fGC += SP + "std::vector<int64_t> fTensor_" + i.first  + " = std::vector<int64_t>(" + std::to_string(length) + ");\n";
            fGC += SP + "int64_t * tensor_" + i.first + " = fTensor_" + i.first  + ".data();\n";
         }
         fGC += "\n";
      }

      if (fUseSession) {
         // add here specific operator code that needs to define session data members
         fGC += "\n";
         for (size_t id = 0; id < fOperators.size(); id++) {
            std::string opName = std::to_string(id);
            fGC += fOperators[id]->GenerateSessionMembersCode(opName);
         }
         fGC += "\n";
         // here add initialization and reading of weight tensors
         if (fUseWeightFile) {
            fGC += "Session(std::string filename =\"\") {\n";
            fGC += "   if (filename.empty()) filename = \"" + fName;
            if (fWeightFile == WeightFileType::Text) {
               fGC += ".dat\";\n";
            }
            if (fWeightFile == WeightFileType::RootBinary) {
               fGC += ".root\";\n";
            }
            ReadInitializedTensorsFromFile();
            //fUseWeightFile = fUseWeightFile;
         } else {
            // no need to pass weight file since it is not used
            // keep passing a string for compatibility
            fGC += "Session(std::string = \"\") {\n";
         }
         // add here initialization code
         for (size_t id = 0; id < fOperators.size() ; id++){
            fGC += fOperators[id]->GenerateInitCode();
         }
         fGC += "}\n\n";
      }

      size_t outputSize = fOutputTensorNames.size();
      // assume output types are all the same
      std::string outputType;
      if (outputSize == 1) {
         auto f = fIntermediateTensorInfos.find(fOutputTensorNames[0]);
         if (f == fIntermediateTensorInfos.end()){
            throw std::runtime_error("TMVA-SOFIE: output tensor " + fOutputTensorNames[0] + " not found when trying to get its info");
         }else{
            outputType = ConvertTypeToString(f->second.type);
            fGC += "std::vector<" + outputType + "> ";
         }
      } else {
         std::vector<ETensorType> outputTensorsTypes(outputSize);
         for (size_t i = 0; i < outputSize; i++) {
            auto f = fIntermediateTensorInfos.find(fOutputTensorNames[i]);
            if (f == fIntermediateTensorInfos.end()) {
               throw std::runtime_error("TMVA-SOFIE: output tensor " + fOutputTensorNames[i]
                  + " not found when trying to get its info");
            } else {
               outputTensorsTypes[i] = f->second.type;
            }
         }
         // assume all output types are the same
         outputType = ConvertTypeToString(outputTensorsTypes[0]);
         for (size_t i = 0; i < outputSize; i++) {
            if (outputTensorsTypes[i] != outputTensorsTypes[0]) {
               throw std::runtime_error("TMVA-SOFIE: output tensor " + fOutputTensorNames[i] + " is of different type.");
            }
         }
         fGC += "std::vector<std::vector<" + outputType + ">> ";
      }

      fGC += "infer(";
       for(size_t i = 0; i<fInputTensorNames.size(); ++i){
         switch((fReadyInputTensorInfos[fInputTensorNames[i]]).type){
            case  ETensorType::FLOAT :{
               fGC += "std::vector<float> fTensor_" + fInputTensorNames[i] + ",";
               break;
            }
            case  ETensorType::INT32 :{
               fGC += "std::vector<int32_t> fTensor_" + fInputTensorNames[i] + ",";
               break;
            }
            case  ETensorType::INT64 :{
               fGC += "std::vector<int64_t> fTensor_" + fInputTensorNames[i] + ",";
               break;
            }
            case  ETensorType::DOUBLE :{
               fGC += "std::vector<double> fTensor_" + fInputTensorNames[i] + ",";
               break;
            }
            default: {
               throw std::runtime_error("TMVA-SOFIE: input tensor " + fInputTensorNames[i] + " is of a data type which is not yet supported.");
            }
         }
      }
      fGC.pop_back(); //remove last ","
      fGC += "){\n\n";

      // Create output vectors
      if (outputSize == 1) {
         size_t outputLength = ConvertShapeToLength(GetTensorShape(fOutputTensorNames[0]));
         fGC += SP + "std::vector<" + outputType + "> ret(" + std::to_string(outputLength) + ");\n";
      }
      else {
         for (size_t i=0; i<outputSize; i++) {
            if (!fOutputTensorNames[i].empty()) {
               size_t outputLength = ConvertShapeToLength(GetTensorShape(fOutputTensorNames[i]));
               fGC += SP + "std::vector<" + outputType + "> ret_";
               fGC += std::to_string(i);
               fGC += "(" + std::to_string(outputLength) + ");\n";
            }
         }
         fGC += SP + "std::vector<std::vector<" + outputType + ">> ret({";
         for (size_t i=0; i<outputSize; i++) {
            if (fOutputTensorNames[i].empty()) {
               fGC += "{}";
            }
            else {
               fGC += "ret_";
               fGC += std::to_string(i);
            }
            if (i<outputSize - 1) {
               fGC += ", ";
            }
         }
         fGC += "});\n";
      }

      fGC += SP + "try {\n"; //beginning of try-catch block
      
      // Lambda device selector
      fGC += SP*2 + "auto custom_gpu_selector = [](const cl::sycl::device& dev) {\n";
      fGC += SP*3 + "if (dev.has(cl::sycl::aspect::gpu)) {\n";
      fGC += SP*4 + "auto vendorName = dev.get_info<cl::sycl::info::device::vendor>();\n";
      switch(target_gpu) {
         case Intel: {fGC += SP*4 + "if (vendorName.find(\"Intel\") != std::string::npos) {\n"; break;}
         case NVIDIA: {fGC += SP*4 + "if (vendorName.find(\"NVIDIA\") != std::string::npos) {\n"; break;}
         case AMD: {fGC += SP*4 + "if (vendorName.find(\"AMD\") != std::string::npos) {\n"; break;}
      }
      fGC += SP*5 + "return 1;\n";
      fGC += SP*4 + "}\n";
      fGC += SP*3 + "}\n";
      fGC += SP*3 + "return -1;\n" + SP*2 + "};\n\n";

      // Create queue
      fGC += SP + "// Create Queue\n";
      fGC += SP*2 + "auto q = cl::sycl::queue{custom_gpu_selector, [=](cl::sycl::exception_list eL){\n";
      //fGC += SP*2 + "for (auto e:eL) {std::rethrow_exception(e);}}, cl::sycl::property::queue::in_order{}};\n";
      fGC += SP*2 + "for (auto e:eL) {std::rethrow_exception(e);}}};\n";

      fGC += SP*3 + "const sycl::property_list props = {sycl::property::buffer::use_host_ptr()};\n";

      if (gpu_blas == portBLAS) {
         fGC += SP*3 + "blas::SB_Handle sb_handle(q);\n";
      }
     
      // Buffer Creation
      fGC += SP*2 + "{\n"; // start of buffer creation

      
      // Create buffers for inputs
      for (size_t i=0; i<fInputTensorNames.size(); ++i) {
         fGC += SP*3 + "auto buf_tensor_" + fInputTensorNames[i] + " = cl::sycl::buffer{fTensor_" + fInputTensorNames[i];
         fGC += ".data(), cl::sycl::range{fTensor_" + fInputTensorNames[i] + ".size()}, props};\n";
         fGC += SP*3 + "buf_tensor_" + fInputTensorNames[i] + ".set_final_data(nullptr);\n";
      }

      // Create buffers for initialized tensors
      for (auto& i : fInitializedTensors) {
         if (i.second.fType == ETensorType::FLOAT){
            fGC += SP*3 + "auto buf_tensor_" + i.first + " = cl::sycl::buffer{fTensor_" + i.first + ".data(), cl::sycl::range{fTensor_" + i.first + ".size()}, props};\n";
            fGC += SP*3 + "buf_tensor_" + i.first + ".set_final_data(nullptr);\n";
         }
      }

      fGC += "\n";

      // Create buffers for intermediate tensors
      for (auto& i : fIntermediateTensorInfos) {
         fGC += SP*3 + "auto buf_tensor_" + i.first + " = cl::sycl::buffer{fTensor_" + i.first + ".data(), cl::sycl::range{fTensor_" + i.first + ".size()}, props};\n";
         
         // if the intermediate tensor is not an output
         if (std::find(fOutputTensorNames.begin(), fOutputTensorNames.end(), i.first) == fOutputTensorNames.end()) {
            fGC += SP*3 + "buf_tensor_" + i.first + ".set_final_data(nullptr);\n";
         }
      }

      if (outputSize == 1) {
         fGC += SP*3 + "buf_tensor_" + fOutputTensorNames[0] + ".set_final_data(ret.data());\n";
         fGC += SP*3 + "buf_tensor_" + fOutputTensorNames[0] + ".set_write_back(true);\n";
      }
      else {
         for (size_t i=0; i<outputSize; i++) {
            if (!fOutputTensorNames[i].empty()) {
               fGC += SP*3 + "buf_tensor_" + fOutputTensorNames[i] + ".set_final_data(ret_" + std::to_string(i) + ".data());\n";
               fGC += SP*3 + "buf_tensor_" + fOutputTensorNames[i] + ".set_write_back(true);\n";
            }
         }
      }

      for (size_t id = 0; id < fOperators.size() ; id++){
         fGC+= (fOperators[id]->GenerateGPU(std::to_string(id), gemm, copy, axpy, transpose, nontrans, trans, copy_batch, scal));
      }

      fGC += "\n";
      fGC += SP*3 + "q.wait_and_throw();\n"; // call wait to enforce in order
      fGC += SP*2 + "}\n"; // buffer destruction

      fGC += SP + "}\n"; // end of try clause
   
      fGC += SP + "catch (const cl::sycl::exception& e) {\n"; //beginning of catch clause
      fGC += SP*2 + "std::cout << \"Exception caught: \" << e.what() << ";
      fGC += "\"with OpenCL error code: \" << e.code() << std::endl;\n";
      fGC += SP + "}\n"; //end of catch clause

      if (outputSize != 1) {
         fGC += "ret = {";
         for (size_t i = 0; i < outputSize; i++) {
            if (fOutputTensorNames[i].empty()) {
               fGC += "{}";
            } else {
               fGC += "ret_";
               fGC += std::to_string(i);
            }
            if (i < outputSize - 1) {
               fGC += ",";
            }
         }
         fGC += "};\n";
      }
      
      fGC += SP + "return ret;\n";
      fGC += "}\n";
      if (fUseSession) {
         fGC += "};\n";
      }
      fGC += ("} //TMVA_SOFIE_" + fName + "\n");
      fGC += "\n#endif  // " + hgname + "\n";

      fGC_GPU = fGC;
   }

   void RModel::ReadInitializedTensorsFromFile() {
      // generate the code to read initialized tensors from a text data file
      if (fWeightFile == WeightFileType::Text) {
         if (fInitializedTensors.empty()) return;

         fGC += "   std::ifstream f;\n";
         fGC += "   f.open(filename);\n";
         fGC += "   if (!f.is_open()) {\n";
         fGC += "      throw std::runtime_error(\"tmva-sofie failed to open file for input weights\");\n";
         fGC += "   }\n";
         fGC += "   std::string tensor_name;\n";
         fGC += "   size_t length;\n";

         // loop on tensors and parse the file
         for (auto& i: fInitializedTensors) {
            if (i.second.fType == ETensorType::FLOAT) {
               size_t length = 1;
               length = ConvertShapeToLength(i.second.fShape);
               std::string tensor_name = "tensor_" + i.first;
               std::string slength = std::to_string(length);
               fGC += "   f >> tensor_name >> length;\n";
               fGC += "   if (tensor_name != \"" + tensor_name + "\" ) {\n";
               fGC += "      std::string err_msg = \"TMVA-SOFIE failed to read the correct tensor name; expected name is " +
                     tensor_name + " , read \" + tensor_name;\n";
               fGC += "      throw std::runtime_error(err_msg);\n";
               fGC += "    }\n";
               fGC += "   if (length != " + slength + ") {\n";
               fGC += "      std::string err_msg = \"TMVA-SOFIE failed to read the correct tensor size; expected size is " +
                     slength + " , read \" + std::to_string(length) ;\n";
               fGC += "      throw std::runtime_error(err_msg);\n";
               fGC += "    }\n";
               fGC += "   for (size_t i = 0; i < length; ++i)\n";
               fGC += "      f >> " + tensor_name + "[i];\n";
         }
      }
      fGC += "   f.close();\n";
   }

   // generate the code to read initialized tensors from a ROOT data file
   if(fWeightFile == WeightFileType::RootBinary) {
      fGC += "   {\n";
      fGC += "   std::unique_ptr<TFile> rootFile(TFile::Open(filename.c_str(), \"READ\"));\n";
      fGC += "   if (!rootFile->IsOpen()) {\n";
      fGC += "      throw std::runtime_error(\"tmva-sofie failed to open ROOT file for input weights\");\n";
      fGC += "   }\n";

      std::string dirName = fName + "_weights";
      fGC += "   if (!rootFile->GetKey(\"" + dirName + "\")) {\n";
      fGC += "      throw std::runtime_error(\"tmva-sofie failed to open ROOT directory for input weights\");\n";
      fGC += "   }\n";

      for (auto &i : fInitializedTensors) {
         fGC += "  {\n";
         std::string tensor_name = "tensor_" + i.first;
         if (i.second.fType == ETensorType::FLOAT) {
            fGC += "      fTensor_" + i.first + " = *reinterpret_cast<std::vector<float>*>(rootFile->Get(\"";
            fGC += dirName + "/" + tensor_name + "\"));\n";
         } else if (i.second.fType == ETensorType::DOUBLE) {
            fGC += "      fTensor_" + i.first + " = *reinterpret_cast<std::vector<double>*>(rootFile->Get(\"";
            fGC += dirName + + "/" + tensor_name + "\"));\n";
         } else if (i.second.fType == ETensorType::INT64) {
            fGC += "      fTensor_" + i.first + " = *reinterpret_cast<std::vector<int64_t>*>(rootFile->Get(\"";
            fGC += dirName + "/" + tensor_name + "\"));\n";
         }
         fGC += "  }\n";
      }
      fGC += "  }\n";
   }
}

void RModel::WriteInitializedTensorsToFile(std::string filename) {
   // Determine the file extension based on the weight file type
   std::string fileExtension;
   switch (fWeightFile) {
      case WeightFileType::None:
         fileExtension = ".dat";
         break;
      case WeightFileType::RootBinary:
         fileExtension = ".root";
         break;
      case WeightFileType::Text:
         fileExtension = ".dat";
         break;
   }

   // If filename is empty, use the model name as the base filename
   if (filename.empty()) {
      filename = fFileName + fileExtension;
   }

   // Write the initialized tensors to the file
   if (fWeightFile == WeightFileType::RootBinary) {
      std::unique_ptr<TFile> outputFile(TFile::Open(filename.c_str(), "UPDATE"));

      std::string dirName = fName + "_weights";
      // check if directory exists, in case delete to replace with new one
      if (outputFile->GetKey(dirName.c_str()))
         outputFile->rmdir(dirName.c_str());

      auto outputDir = outputFile->mkdir(dirName.c_str());

      for (const auto& item : fInitializedTensors) {
         std::string tensorName = "tensor_" + item.first;
         size_t length = 1;
         length = ConvertShapeToLength(item.second.fShape);
         if(item.second.fType == ETensorType::FLOAT){
            const std::shared_ptr<void> ptr = item.second.fData; // shared_ptr<void> instance
            const float* data = (std::static_pointer_cast<float>(item.second.fData)).get();
            std::vector<float> tensorDataVector(data , data + length);
            outputDir->WriteObjectAny(&tensorDataVector, "std::vector<float>", tensorName.c_str());
         }
         else if(item.second.fType == ETensorType::DOUBLE){
            const std::shared_ptr<void> ptr = item.second.fData; // shared_ptr<void> instance
            const double* data = (std::static_pointer_cast<double>(item.second.fData)).get();
            std::vector<double> tensorDataVector(data , data + length);
            outputDir->WriteObjectAny(&tensorDataVector, "std::vector<double>", tensorName.c_str());
         }
         else if(item.second.fType == ETensorType::INT64) {
            const std::shared_ptr<void> ptr = item.second.fData; // shared_ptr<void> instance
            const int64_t* data = (std::static_pointer_cast<int64_t>(item.second.fData)).get();
            std::vector<int64_t> tensorDataVector(data , data + length);
            outputDir->WriteObjectAny(&tensorDataVector, "std::vector<int64_t>", tensorName.c_str());
         }
      }
      outputFile->Write(filename.c_str());
   }

   // Write the initialized tensors to a text file
   if (fWeightFile == WeightFileType::Text) {
      std::ofstream f;
      f.open(filename);
      if (!f.is_open())
         throw std::runtime_error("tmva-sofie failed to open file for tensor weight data");
      for (auto& i: fInitializedTensors){
         if (i.second.fType == ETensorType::FLOAT){
            size_t length = 1;
            for (auto &dim : i.second.fShape) {
               length *= dim;
            }
            std::string tensor_name = "tensor_" + i.first;
            f << tensor_name << " " << length << "\n";
            const float * data = (std::static_pointer_cast<float>(i.second.fData)).get();
            for (size_t idx = 0; idx < length - 1; idx++) {
               f << std::setprecision(std::numeric_limits<float>::max_digits10) << data[idx] << " ";
            }
            f << std::setprecision(std::numeric_limits<float>::max_digits10) << data[length - 1];
            f << "\n";
         }
      }
      f.close();
   }
}

   void RModel::PrintRequiredInputTensors(){
      std::cout << "Model requires following inputs:\n";
      for (auto& inputInfo: fInputTensorInfos){
         std::cout << "Parameterised Tensor name: " << inputInfo.first << "\t";
         std::cout << "type: " << ConvertTypeToString(inputInfo.second.type) << "\t";
         std::cout << "shape: [";
         for (size_t i = 0; i < inputInfo.second.shape.size(); i++){
            if (inputInfo.second.shape[i].isParam){
               std::cout << inputInfo.second.shape[i].param;
            }else{
               std::cout << inputInfo.second.shape[i].dim ;
            }
            if (i < inputInfo.second.shape.size() - 1) std::cout << ",";
         }
         std::cout << "]" << std::endl;
      }

      for (auto& inputInfo: fReadyInputTensorInfos){
         std::cout << "Fully Specified Tensor name: " << inputInfo.first << "\t";
         std::cout << "type: " << ConvertTypeToString(inputInfo.second.type) << "\t";
         std::cout << "shape: [";
         for (size_t i = 0; i < inputInfo.second.shape.size(); i++){
            std::cout << inputInfo.second.shape[i];
            if (i < inputInfo.second.shape.size() - 1) std::cout << ",";
         }
         std::cout << "]" << std::endl;
      }

   }

   void RModel::PrintInitializedTensors(){
      std::cout << "Model initialized the following tensors:\n";
      for (auto& it: fInitializedTensors){
         std::cout << "Tensor name: \"" << it.first << "\"\t";
         std::cout << "type: " << ConvertTypeToString(it.second.fType) << "\t";
         std::cout << "shape: [";
         for (size_t i = 0; i < it.second.fShape.size(); i++){
            std::cout << it.second.fShape[i];
            if (i < it.second.fShape.size() - 1) std::cout << ",";
         }
         std::cout << "]" << std::endl;
      }
   }

   void RModel::PrintIntermediateTensors(){
      std::cout << "Model specify the following intermediate tensors:\n";
      for (auto& it: fIntermediateTensorInfos){
         std::cout << "Tensor name: \"" << it.first << "\"\t";
         std::cout << "type: " << ConvertTypeToString(it.second.type) << "\t";
         std::cout << "shape: [";
         for (size_t i = 0; i < it.second.shape.size(); i++){
            std::cout << it.second.shape[i];
            if (i < it.second.shape.size() - 1) std::cout << ",";
         }
         std::cout << "]" << std::endl;
      }
   }

   void RModel::HeadInitializedTensors(std::string name, int n_print){
      auto it = fInitializedTensors.find(name);
      if (it == fInitializedTensors.end()){
         std::cout << "Tensor " << name << " not found in model's intialized tensor list" << std::endl;
         return;
      }

      std::cout << "Tensor name: " << it->first << "\t";
      std::cout << "type: " << ConvertTypeToString(it->second.fType) << "\t";
      int length =1;
      std::cout << "shape: [";
      for (size_t i = 0; i < it->second.fShape.size(); i++){
         std::cout << it->second.fShape[i];
         length *= it->second.fShape[i];
         if (i < it->second.fShape.size() - 1) std::cout << ",";
      }
      std::cout << "]" << std::endl;
      bool ellipsis = true;
      if (n_print > length){
         n_print = length;
         ellipsis = false;
      }

      std::cout << "data: [" << std::endl;
      //switch(it->second.type){
      //   case ETensorType::FLOAT : {
      if (it->second.fType == ETensorType::FLOAT) {
         auto converted_data = std::static_pointer_cast<float>(it->second.fData).get();
         for (int i =0; i < n_print; i++){
            std::cout << converted_data[i];
            if (i < n_print - 1) std::cout << " ,";
         }
         //   break;
         // }
      }
      if (ellipsis) std::cout << ", ...";
      std::cout << "]" << std::endl;

   }

   void RModel::OutputGenerated(std::string filename, bool append) {
      // the model can be appended only if a file name is provided
      if (filename.empty()) {
         // if a file is pr
         filename = fName + ".hxx";
         append = false;
      }
      if (append && fWeightFile != WeightFileType::RootBinary) {
         std::cout << "weights can be appended only when using ROOT binary files" << std::endl;
         append = false;
      }
      std::ofstream f;
      if (append)
         f.open(filename, std::ios_base::app);
      else
         f.open(filename);
      if (!f.is_open()) {
         throw
            std::runtime_error("tmva-sofie failed to open file for output generated inference code");
      }
      f << fGC_CPU;
      f.close();

      // write weights in a text file
      size_t pos = filename.find(".hxx");
      filename.replace(pos,4,".dat");
      if (fUseWeightFile) WriteInitializedTensorsToFile(filename);
   }

   void RModel::OutputGeneratedGPU(std::string filename){
      if (filename == ""){
         filename = fName + ".hxx";
      }
      std::ofstream f;
      f.open(filename);
      if (!f.is_open()){
         throw std::runtime_error("tmva-sofie failed to open file for output generated inference code");
      }
      f << fGC_GPU;
      f.close();

      // write weights in a text or root binary file
      if (fUseWeightFile) {
         size_t pos = filename.find(".hxx");
         if (fWeightFile == WeightFileType::Text)
            filename.replace(pos, 4, ".dat");
         if (fWeightFile == WeightFileType::RootBinary)  {
            filename = filename.erase(pos, 4);
            filename += ".root";
         }
         WriteInitializedTensorsToFile(filename);
      }
   }

   void RModel::Streamer(TBuffer &R__b){
       if (R__b.IsReading()) {
           RModel::Class()->ReadBuffer(R__b, this);
           for(auto i=RModel::fInitializedTensors.begin(); i!=RModel::fInitializedTensors.end();++i){
               i->second.CastPersistentToShared();
           }
       }
       else {
          for(auto i=RModel::fInitializedTensors.begin(); i!=RModel::fInitializedTensors.end();++i){
               i->second.CastSharedToPersistent();
           }
          RModel::Class()->WriteBuffer(R__b, this);
       }
   }

}//SOFIE
}//Experimental
}//TMVA
