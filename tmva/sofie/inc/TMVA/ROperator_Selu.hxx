#ifndef TMVA_SOFIE_ROPERATOR_SELU
#define TMVA_SOFIE_ROPERATOR_SELU

#include "TMVA/SOFIE_common.hxx"
#include "TMVA/ROperator.hxx"
#include "TMVA/RModel.hxx"

#include <sstream>

namespace TMVA{
namespace Experimental{
namespace SOFIE{

template <typename T>
class ROperator_Selu final : public ROperator
{

private:

   std::string fNX;
   std::string fNY;
   std::vector<size_t> fShape;

public:
   ROperator_Selu(){}
   ROperator_Selu(std::string nameX, std::string nameY):
      fNX(UTILITY::Clean_name(nameX)), fNY(UTILITY::Clean_name(nameY)){}

   std::vector<ETensorType> TypeInference(std::vector<ETensorType> input){
      return input;
   }

   std::vector<std::vector<size_t>> ShapeInference(std::vector<std::vector<size_t>> input){
      auto ret = input; //suggest copy to compiler
      return ret;
   }

   void Initialize(RModel& model){
      if (model.CheckIfTensorAlreadyExist(fNX) == false){   //input must be a graph input, or already initialized intermediate tensor
         throw std::runtime_error("TMVA SOFIE Selu Op Input Tensor is not found in model");
      }
      fShape = model.GetTensorShape(fNX);
      model.AddIntermediateTensor(fNY, model.GetTensorType(fNX), fShape);
   }


   std::string Generate(std::string OpName){
      OpName = "op_" + OpName;
      if (fShape.empty()){
         throw std::runtime_error("TMVA SOFIE Operator Selu called to Generate without being initialized first");
      }
      std::stringstream out;
      int length = 1;
      for(auto& i: fShape){
         length *= i;
      }
      out << "\t" << "for (int id = 0; id < " << length << " ; id++){\n";
      out << "\t\t" << "tensor_" << fNY << "[id] = 1.0507009873554804934193349852946 * (std::max(float(0.0), tensor_"  << fNX << "[id]) + std::min(0.0, 1.6732632423543772848170429916717 * (std::exp(" << "tensor_" << fNX << "[id]" <<")-1)));\n";
      out << "\t}\n";
      return out.str();
   }

   std::string GenerateGPU(std::string OpName, std::string gemm, std::string copy, 
   std::string axpy, std::string transpose, std::string nontrans, std::string trans, std::string copy_batch, std::string scal) {
      OpName = "op_" + OpName;
      if (fShape.empty()) {
         throw std::runtime_error("TMVA SOFIE Operator Selu called to Generate without being initialized first");
      }
      std::stringstream out;
      int length = 1;
      for(auto& i: fShape){
         length *= i;
      }

      out << "\n" << SP*3 << "//------ SELU\n";
      out << SP*3 << "q.submit([&](cl::sycl::handler &cgh){\n";
      out << SP*4 << "auto acc_tensor_" << fNX << " = cl::sycl::accessor{buf_tensor_" << fNX;
      out << ", cgh, cl::sycl::read_only};\n";
      out << SP*4 << "auto acc_tensor_" << fNY << " = cl::sycl::accessor{buf_tensor_" << fNY;
      out << ", cgh, cl::sycl::write_only, cl::sycl::no_init};\n\n";
      out << SP*4 << "cgh.parallel_for<class " << OpName << ">(cl::sycl::range<1>(" << std::to_string(length);
      out << "), [=](cl::sycl::id<1> id){\n";
      out << SP*5 << "acc_tensor_" << fNY << "[id] = 1.0507009873554804934193349852946 * ";
      out << "(cl::sycl::max(static_cast<float>(0.0), acc_tensor_" << fNX << "[id]) + cl::sycl::min(static_cast<float>(0.0), ";
      out << "static_cast<float>(1.6732632423543772848170429916717) * (cl::sycl::exp(acc_tensor_" << fNX << "[id])-1)));\n";        
      out << SP*4 << "});\n";
      out << SP*3 << "});\n";

      return out.str();
   }

   std::vector<std::string> GetStdLibs() { return { std::string("cmath") };}
};

}//SOFIE
}//Experimental
}//TMVA


#endif //TMVA_SOFIE_ROPERATOR_SELU
