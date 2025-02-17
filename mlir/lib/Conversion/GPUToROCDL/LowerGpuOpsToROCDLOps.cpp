//===- LowerGpuOpsToROCDLOps.cpp - MLIR GPU to ROCDL lowering passes ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a pass to generate ROCDLIR operations for higher-level
// GPU operations.
//
//===----------------------------------------------------------------------===//

#include "mlir/Conversion/GPUToROCDL/GPUToROCDLPass.h"

#include "mlir/Conversion/StandardToLLVM/ConvertStandardToLLVMPass.h"
#include "mlir/Dialect/GPU/GPUDialect.h"
#include "mlir/Dialect/LLVMIR/ROCDLDialect.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"

#include "../GPUCommon/IndexIntrinsicsOpLowering.h"
#include "../GPUCommon/OpToFuncCallLowering.h"

using namespace mlir;

namespace {

// A pass that replaces all occurrences of GPU device operations with their
// corresponding ROCDL equivalent.
//
// This pass only handles device code and is not meant to be run on GPU host
// code.
class LowerGpuOpsToROCDLOpsPass
    : public OperationPass<LowerGpuOpsToROCDLOpsPass, gpu::GPUModuleOp> {
public:
  void runOnOperation() override {
    gpu::GPUModuleOp m = getOperation();

    OwningRewritePatternList patterns;
    LLVMTypeConverter converter(m.getContext());
    populateStdToLLVMConversionPatterns(converter, patterns);
    patterns.insert<
        GPUIndexIntrinsicOpLowering<gpu::ThreadIdOp, ROCDL::ThreadIdXOp,
                                    ROCDL::ThreadIdYOp, ROCDL::ThreadIdZOp>,
        GPUIndexIntrinsicOpLowering<gpu::BlockDimOp, ROCDL::BlockDimXOp,
                                    ROCDL::BlockDimYOp, ROCDL::BlockDimZOp>,
        GPUIndexIntrinsicOpLowering<gpu::BlockIdOp, ROCDL::BlockIdXOp,
                                    ROCDL::BlockIdYOp, ROCDL::BlockIdZOp>,
        GPUIndexIntrinsicOpLowering<gpu::GridDimOp, ROCDL::GridDimXOp,
                                    ROCDL::GridDimYOp, ROCDL::GridDimZOp>>(
        converter);
    patterns.insert<OpToFuncCallLowering<AbsFOp>>(converter, "_ocml_fabs_f32",
                                                 "_ocml_fabs_f64");
    patterns.insert<OpToFuncCallLowering<CeilFOp>>(converter, "_ocml_ceil_f32",
                                                 "_ocml_ceil_f64");
    patterns.insert<OpToFuncCallLowering<CosOp>>(converter, "_ocml_cos_f32",
                                                 "_ocml_cos_f64");
    patterns.insert<OpToFuncCallLowering<ExpOp>>(converter, "_ocml_exp_f32",
                                                 "_ocml_exp_f64");

    ConversionTarget target(getContext());
    target.addLegalDialect<LLVM::LLVMDialect, ROCDL::ROCDLDialect>();
    target.addIllegalOp<LLVM::FAbsOp, LLVM::FCeilOp, LLVM::CosOp,
                        LLVM::ExpOp>();
    target.addDynamicallyLegalOp<FuncOp>(
        [&](FuncOp op) { return converter.isSignatureLegal(op.getType()); });
    if (failed(applyPartialConversion(m, target, patterns, &converter)))
      signalPassFailure();
  }
};

} // anonymous namespace

std::unique_ptr<OpPassBase<gpu::GPUModuleOp>>
mlir::createLowerGpuOpsToROCDLOpsPass() {
  return std::make_unique<LowerGpuOpsToROCDLOpsPass>();
}

static PassRegistration<LowerGpuOpsToROCDLOpsPass>
    pass("convert-gpu-to-rocdl",
         "Generate ROCDL operations for gpu operations");
