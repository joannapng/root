#ifndef TMVA_SOFIE_ROPERATOR_Reduce
#define TMVA_SOFIE_ROPERATOR_Reduce

#include "TMVA/SOFIE_common.hxx"
#include "TMVA/ROperator.hxx"
#include "TMVA/RModel.hxx"

#include <memory>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <cassert>

namespace TMVA{
namespace Experimental{
namespace SOFIE{

enum EReduceOpMode { ReduceMean, ReduceSumsquare, ReduceProd, InvalidReduceOp };

template <typename T, EReduceOpMode Op>
class ROperator_Reduce final : public ROperator
{
private:
    /* Attributes*/
    int fkeepdims = 1; //default value
    int fAttrAxes;
    EReduceOpMode fReduceOpMode;
    std::string fNX;
    std::string fNY;
    std::vector<size_t> fShapeX;
    std::vector<size_t> fShapeY;


public:

   std::string Name() {
      if (fReduceOpMode == ReduceMean)  return "ReduceMean";
      else if (fReduceOpMode == ReduceSumsquare )  return "ReduceSumsquare";
      else if (fReduceOpMode == ReduceProd ) return "ReduceProd";
      return "Invalid";
   }

   ROperator_Reduce(){}
   ROperator_Reduce(int keepdims,int attrAxes,std::string nameX, std::string nameY):
   fkeepdims(keepdims), fAttrAxes(attrAxes), fNX(UTILITY::Clean_name(nameX)), fNY(UTILITY::Clean_name(nameY)) {
      fReduceOpMode = Op;
   }

   // type of output given input
   std::vector<ETensorType> TypeInference(std::vector<ETensorType> input){
      return input;
   }

   // shape of output tensors given input tensors
   std::vector<std::vector<size_t>> ShapeInference(std::vector<std::vector<size_t>> input){
      auto ret = input; //suggest copy to compiler
      ret[0][fAttrAxes] = 1;
      return ret;
   }
    void Initialize(RModel& model){

        fUseSession = model.UseSession();

        if (model.CheckIfTensorAlreadyExist(fNX) == false){   //input must be a graph input, or already initialized intermediate tensor
            throw std::runtime_error("TMVA SOFIE Reduce Op Input Tensor " + fNX + " is not found in model");
        }
        fShapeX = model.GetTensorShape(fNX);
         // find shape of Y and add it in the list of intermediate tensors
         fShapeY = ShapeInference({fShapeX})[0];
         model.AddIntermediateTensor(fNY, model.GetTensorType(fNX), fShapeY);

    }

    std::string Generate(std::string OpName){
      OpName = "op_" + OpName;
      if (fShapeX.empty() || fShapeY.empty()) {
         throw std::runtime_error("TMVA SOFIE Reduce Op called to Generate without being initialized first");
      }

      size_t outputLength = TMVA::Experimental::SOFIE::ConvertShapeToLength(fShapeY);

      auto inputStrides = TMVA::Experimental::SOFIE::UTILITY::ComputeStrideFromShape(fShapeX);
      auto outputStrides = TMVA::Experimental::SOFIE::UTILITY::ComputeStrideFromShape(fShapeY);

      // write here according to size of shape
      // in generation code can be done automatically
      // i0 =  i / s0 ; i1 = (i % s0) / s1 ; i2 = ( (i % s0) % s1 ) / s2 and so on
      // and we have for the inverse
      // i = i0 * s0 + i1 * s1 + i2 * s2 + i3 * s3 ....

      // don't need to divide by last stride s[n-1] since it is 1 by definition

      std::stringstream out;
      out << "\n//----  operator " << Name() << "  " << OpName << "\n";
      out << SP << "for (size_t i = 0; i < " << outputLength << "; i++) {\n";

      size_t dim = fShapeX.size();   // this is the input dimension (e.g. 2, 3 or 4 or more)

      // here we find output indices
      out << SP << SP << "size_t idx_0 = i / " << outputStrides[0] << ";\n" ;
      out << SP << SP << "size_t itmp = i;\n";
      for (size_t k = 1; k < dim; k++) {
         out << SP << SP << "itmp = itmp % " << outputStrides[k-1] << ";\n" ;
         if (k < dim-1)
            out << SP << SP << "size_t idx_" << k << " = itmp / " << outputStrides[k] << ";\n" ;
         else
           // to avoid division by 1 which is outputStrides[dim-1]
           out << SP << SP << "size_t idx_" << k << " = itmp;\n";
      }

      // compute reduction

      if(fReduceOpMode == ReduceProd)
         out << SP << SP << "float sum = 1;\n";
      else 
         out << SP << SP << "float sum = 0;\n";
      
      out << SP << SP << "for (size_t k = 0; k < " << fShapeX[fAttrAxes] <<"; k++) { \n";
      out << SP << SP << SP << "idx_" << fAttrAxes << " = k;\n";
       // compute input index j
      out << SP << SP << SP << "size_t l = ";
      for(int n = dim-1; n >=0; n--) {
         if (n == int(dim-1))
            out << "idx_" << n;
         else
            out << " + " << "idx_" << n << " * " << inputStrides[n];
      }
      out << ";\n";

      if(fReduceOpMode == ReduceMean){
         out << SP << SP << SP << "sum += tensor_" << fNX << "[l];\n";
         out << SP << SP << "}\n";
         out << SP << SP << "float reduceResult = sum/static_cast<float>(" << fShapeX[fAttrAxes] << ");\n";
      }
      else if(fReduceOpMode == ReduceSumsquare){
         out << SP << SP << SP << "sum += tensor_" << fNX << "[l] * tensor_" << fNX << "[l];\n";
         out << SP << SP << "}\n";
         out << SP << SP << "float reduceResult = sum;\n";
      }
      else if(fReduceOpMode == ReduceProd){
         out << SP << SP << SP << "sum *= tensor_" << fNX << "[l];\n";
         out << SP << SP << "}\n";
         out << SP << SP << "float reduceResult = sum;\n";
      }

      out << SP << SP << "tensor_" << fNY << "[i] = reduceResult;\n";
      out << SP << "}\n";
      return out.str();
   }

   std::string GenerateGPU(std::string OpName, std::string gemm, std::string copy, 
   std::string axpy, std::string transpose, std::string nontrans, std::string trans, std::string copy_batch, std::string scal) {
      OpName = "op_" + OpName;
      if (fShapeX.empty() || fShapeY.empty()) {
         throw std::runtime_error("TMVA SOFIE Reduce Op called to Generate without being initialized first");
      }

      size_t outputLength = TMVA::Experimental::SOFIE::ConvertShapeToLength(fShapeY);

      auto inputStrides = TMVA::Experimental::SOFIE::UTILITY::ComputeStrideFromShape(fShapeX);
      auto outputStrides = TMVA::Experimental::SOFIE::UTILITY::ComputeStrideFromShape(fShapeY);

      std::stringstream out;
      out << "\n" << SP*3 << "//----  operator " << Name() << "  " << OpName << "\n";
      
      out << SP*3 << "q.submit([&](cl::sycl::handler& cgh){\n";
      out << SP*4 << "auto acc_tensor_" << fNX << " = cl::sycl::accessor{buf_tensor_" << fNX;
      out << ", cgh, cl::sycl::read_only};\n";
      out << SP*4 << "auto acc_tensor_" << fNY << " = cl::sycl::accessor{buf_tensor_" << fNY;
      out << ", cgh, cl::sycl::write_only, cl::sycl::no_init};\n";

      out << SP*4 << "cgh.parallel_for<class " << OpName << ">(cl::sycl::range<1>(" << outputLength << "), [=](cl::sycl::id<1> i){\n";
      size_t dim = fShapeX.size();
      out << SP*5 << "size_t idx_0 = i / " << outputStrides[0] << ";\n";
      out << SP*5 << "size_t itmp = i;\n";

      for (size_t k=1; k < dim; k++) {
         out << SP*5 << "itmp = itmp % " << outputStrides[k-1] << ";\n";
         if (k < dim - 1) {
            out << SP*5 << "size_t idx_" << k << " = itmp / " << outputStrides[k] << ";\n";
         }
         else {
            out << SP*5 << "size_t idx_" << k << " = itmp;\n";
         }
      }

      if (fReduceOpMode == ReduceProd)
         out << SP*5 << "float sum = 1;\n";
      else
         out << SP*5 << "float sum = 0;\n";

      out << SP*5 << "for (size_t k=0; k < " << fShapeX[fAttrAxes] << "; k++) { \n";
      out << SP*6 << "idx_" << fAttrAxes << " = k;\n";
      out << SP*6 << "size_t l = "; 

      for (int n = dim - 1; n >=0; n--) {
         if (n == int(dim - 1))
            out << "idx_" << n;
         else  
            out << " + " << "idx_" << n << " * " << inputStrides[n];
      }

      out << ";\n";

      if (fReduceOpMode == ReduceMean) {
         out << SP*6 << "sum += acc_tensor_" << fNX << "[l];\n";
         out << SP*5 << "}\n";
         out << SP*5 << "float reduceResult = sum/static_cast<float>(" << fShapeX[fAttrAxes] << ");\n";
      }
      else if (fReduceOpMode == ReduceSumsquare) {
         out << SP*6 << "sum += acc_tensor_" << fNX << "[l] * acc_tensor_" << fNX << "[l];\n";
         out << SP*5 << "}\n";
         out << SP*5 << "float reduceResult = sum;\n";
      }
      else if (fReduceOpMode == ReduceProd) {
         out << SP*6 << "sum *= acc_tensor_" << fNX << "[l];\n";
         out << SP*5 << "}\n";
         out << SP*5 << "float reduceResult = sum;\n";
      }

      out << SP*5 << "acc_tensor_" << fNY << "[i] = reduceResult;\n";
      out << SP*5 << "});\n";
      out << SP*4 << "});\n";
      
      return out.str();

   }

};

}//SOFIE
}//Experimental
}//TMVA


#endif //TMVA_SOFIE_ROPERATOR_Reduce

