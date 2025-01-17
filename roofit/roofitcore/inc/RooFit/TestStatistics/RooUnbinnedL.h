/*
 * Project: RooFit
 * Authors:
 *   PB, Patrick Bos, Netherlands eScience Center, p.bos@esciencecenter.nl
 *
 * Copyright (c) 2021, CERN
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted according to the terms
 * listed in LICENSE (http://roofit.sourceforge.net/license.txt)
 */

#ifndef ROOT_ROOFIT_TESTSTATISTICS_RooUnbinnedL
#define ROOT_ROOFIT_TESTSTATISTICS_RooUnbinnedL

#include <RooFit/TestStatistics/RooAbsL.h>
#include <RooGlobalFunc.h>

#include "Math/Util.h" // KahanSum

// forward declarations
class RooAbsPdf;
class RooAbsData;
class RooArgSet;
class RooChangeTracker;
namespace ROOT {
namespace Experimental {
class RooFitDriver;
}
} // namespace ROOT

namespace RooFit {
namespace TestStatistics {

class RooUnbinnedL : public RooAbsL {
public:
   RooUnbinnedL(RooAbsPdf *pdf, RooAbsData *data, RooAbsL::Extended extended = RooAbsL::Extended::Auto,
                RooFit::BatchModeOption batchMode = RooFit::BatchModeOption::Off);
   RooUnbinnedL(const RooUnbinnedL &other);
   ~RooUnbinnedL() override;
   bool setApplyWeightSquared(bool flag);

   ROOT::Math::KahanSum<double>
   evaluatePartition(Section events, std::size_t components_begin, std::size_t components_end) override;

<<<<<<< HEAD
   void setUseBatchedEvaluations(bool flag);

   std::string GetClassName() const override { return "RooUnbinnedL"; }
||||||| 758899052b
   void setUseBatchedEvaluations(bool flag);

   virtual std::string GetClassName() const override { return "RooUnbinnedL"; };
=======
   std::string GetClassName() const override { return "RooUnbinnedL"; }
>>>>>>> master

private:
<<<<<<< HEAD
   bool apply_weight_squared = false; ///< Apply weights squared?
   mutable bool _first = true;        ///<!
   bool useBatchedEvaluations_ = false;
||||||| 758899052b
   bool apply_weight_squared = false;                              ///< Apply weights squared?
   mutable bool _first = true;                                     ///<!
   bool useBatchedEvaluations_ = false;
=======
   bool apply_weight_squared = false; ///< Apply weights squared?
   mutable bool _first = true;        ///<!
>>>>>>> master
   std::unique_ptr<RooChangeTracker> paramTracker_;
<<<<<<< HEAD
   Section lastSection_ = {0, 0}; // used for cache together with the parameter tracker
   mutable ROOT::Math::KahanSum<double> cachedResult_{0.};
||||||| 758899052b
   Section lastSection_ = {0, 0};  // used for cache together with the parameter tracker
   mutable ROOT::Math::KahanSum<double> cachedResult_ = 0;
=======
   Section lastSection_ = {0, 0}; // used for cache together with the parameter tracker
   mutable ROOT::Math::KahanSum<double> cachedResult_{0.};
   std::shared_ptr<ROOT::Experimental::RooFitDriver> driver_; ///<! For batched evaluation
   std::stack<std::vector<double>> _vectorBuffers;            // used for preserving resources in batched evaluation
>>>>>>> master
};

} // namespace TestStatistics
} // namespace RooFit

#endif // ROOT_ROOFIT_TESTSTATISTICS_RooUnbinnedL
