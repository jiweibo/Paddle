/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserve.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/fluid/framework/op_registry.h"
#include "paddle/fluid/framework/operator.h"
#include "paddle/fluid/operators/batch_size_like.h"

namespace paddle {
namespace operators {

class UniformRandomBatchSizeLikeOp : public BatchSizeLikeOp {
 protected:
  using BatchSizeLikeOp::BatchSizeLikeOp;

  framework::OpKernelType GetExpectedKernelType(
      const framework::ExecutionContext &ctx) const override {
    return framework::OpKernelType(
        static_cast<framework::proto::VarType::Type>(ctx.Attr<int>("dtype")),
        ctx.GetPlace());
  }
};

class UniformRandomBatchSizeLikeOpMaker : public BatchSizeLikeOpMaker {
 protected:
  void Apply() override {
    AddComment(R"DOC(
UniformRandomBatchSizeLike operator.

This operator initializes a tensor with the same batch_size as the Input tensor
with random values sampled from a uniform distribution.

)DOC");
    AddAttr<float>("min",
                   "(float, default -1.0) "
                   "Minimum value of uniform random")
        .SetDefault(-1.0f);
    AddAttr<float>("max",
                   "(float, default 1.0) "
                   "Maximun value of uniform random")
        .SetDefault(1.0f);
    AddAttr<int>("seed",
                 "(int, default 0) "
                 "Random seed used for generating samples. "
                 "0 means use a seed generated by the system."
                 "Note that if seed is not 0, this operator will always "
                 "generate the same random numbers every time.")
        .SetDefault(0);
    AddAttr<int>("diag_num",
                 "The number of diag elements. Note that if "
                 "diag_num is 0, it means without diag init.[default 0].")
        .SetDefault(0);
    AddAttr<int>("diag_step", "The step between two diag element.[default 0].")
        .SetDefault(0);
    AddAttr<float>("diag_val", "The value of diag element. [default 1.0].")
        .SetDefault(1.0f);
    AddAttr<int>("dtype", "(int, default 5(FP32)) Output tensor data type")
        .SetDefault(framework::proto::VarType::FP32);
  }
};

}  // namespace operators
}  // namespace paddle

REGISTER_OPERATOR(uniform_random_batch_size_like,
                  paddle::operators::UniformRandomBatchSizeLikeOp,
                  paddle::operators::UniformRandomBatchSizeLikeOpMaker,
                  paddle::operators::BatchSizeLikeNoNeedBufferVarsInferer);
REGISTER_OPERATOR_MAKER(
    uniform_random_batch_size_like,
    paddle::operators::UniformRandomBatchSizeLikeOp,
    paddle::framework::EmptyGradOpMaker<paddle::framework::OpDesc>,
    paddle::framework::EmptyGradOpMaker<paddle::imperative::OpBase>);
// Kernels are registered in uniform_random_op.cc and uniform_random_op.cu
