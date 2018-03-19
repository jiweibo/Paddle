/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/fluid/operators/reshape_op.h"

namespace paddle {
namespace operators {

class ReshapeOp : public framework::OperatorWithKernel {
 public:
  ReshapeOp(const std::string &type, const framework::VariableNameMap &inputs,
            const framework::VariableNameMap &outputs,
            const framework::AttributeMap &attrs)
      : OperatorWithKernel(type, inputs, outputs, attrs) {}

  void InferShape(framework::InferShapeContext *ctx) const override {
    PADDLE_ENFORCE(ctx->HasInput("X"),
                   "Input(X) of ReshapeOp should not be null.");
    PADDLE_ENFORCE(ctx->HasOutput("Out"),
                   "Output(Out) of ReshapeOp should not be null.");

    const std::vector<int> &shape = ctx->Attrs().Get<std::vector<int>>("shape");
    PADDLE_ENFORCE(!shape.empty(),
                   "The shape information must be set by Attr(shape).");

    std::vector<int64_t> output_shape;
    auto x_dims = ctx->GetInputDim("X");
    bool need_copy_dim = ValidateShape(shape, x_dims, output_shape);

    if (need_copy_dim) {
      // Some dimensions can only be determined during runtime. Here temporarily
      // set output tensor's shape the same as that of the input tensor.
      ctx->SetOutputDim("Out", x_dims);
    } else {
      ctx->SetOutputDim("Out", framework::make_ddim(output_shape));
    }

    // NOTE: Reshape op cannot reshape an input sequence batch into an output
    // sequence batch that has a different number of time steps.
    // Here output always shares the LoD information with input. But if
    // Attr(shape) contains 0 or -1, the actual output shape can only be
    // determined during runtime. The check for wheather it is a valid output
    // sequence batch is performed in runtime.
    ctx->ShareLoD("X", /*->*/ "Out");
  }

 private:
  bool ValidateShape(const std::vector<int> &shape,
                     const framework::DDim &input_dim,
                     std::vector<int64_t> &output_shape) const {
    // only one dimension can be set to -1, whose size will be automatically
    // infered.
    const int64_t unknown_index = -1;
    const auto in_size = framework::product(input_dim);
    const auto x_rank = input_dim.size();

    bool need_dim_copy = false;
    std::vector<size_t> neg_dims_idx;
    for (size_t i = 0; i < shape.size(); ++i) {
      PADDLE_ENFORCE(shape[i] >= 0 || shape[i] == unknown_index,
                     "Each input dimension of Attr(shape) must be positive, or "
                     "only one input dimension can be -1.");
      if (shape[i] == unknown_index) {
        neg_dims_idx.push_back(i);
      } else if (shape[i] == 0) {
        PADDLE_ENFORCE_LT(
            i, x_rank,
            "Only dimension less than rank of Input(X) can be set to 0.");
        need_dim_copy = true;
      }
    }
    PADDLE_ENFORCE_LE(
        neg_dims_idx.size(), 1,
        "Only one input dimension of Attr(shape) can be unknown.");

    output_shape.resize(shape.size(), 0);
    std::transform(shape.begin(), shape.end(), output_shape.begin(),
                   [](int a) { return static_cast<int64_t>(a); });

    // some dimension can only be determinted during runtime.
    if (need_dim_copy) return need_dim_copy;

    int64_t inferred_dim = 0;
    if (neg_dims_idx.size()) {
      int64_t capacity = std::accumulate(shape.begin(), shape.end(), 1,
                                         std::multiplies<int>());
      inferred_dim = in_size / (-capacity);
      PADDLE_ENFORCE_EQ(inferred_dim * (-capacity), in_size,
                        "Invalid shape is given.");
      output_shape[neg_dims_idx[0]] = inferred_dim;
    }
    return false;
  }
};

class ReshapeOpMaker : public framework::OpProtoAndCheckerMaker {
 public:
  ReshapeOpMaker(OpProto *proto, OpAttrChecker *op_checker)
      : OpProtoAndCheckerMaker(proto, op_checker) {
    AddInput("X", "The input tensor of reshape operator.");
    AddOutput("Out", "The output tensor of reshape operator.");
    AddAttr<std::vector<int>>(
        "shape", "(std::vector<int>) Target shape of reshape operator.");
    AddAttr<bool>("inplace",
                  "(default: false) Change the source tensor's shape without "
                  "memory copy. When Attr(inplace) is set true, the output "
                  "tensor shares memory with Input(X), otherwise, a new output "
                  "tensor is created, and its data are copied from Input(x).")
        .SetDefault(false);
    AddComment(R"DOC(
Reshape Operator.

Reshape Input(X) into the shape specified by Attr(shape). The data in Input(X)
are unchanged.

Examples:

1. Given a 3-D tensor Input(X) with a shape [2, 4, 6], and the target shape
specified by Attr(shape) is [6, 8], the reshape operator will transform Input(X)
into a 2-D tensor with shape [6, 8] and leaving Input(X)'s data unchanged.

1. Given a 3-D tensor Input(X) with a shape [2, 4, 6], and the target shape
specified by Attr(shape) is [2, 3, -1, 2], the reshape operator will transform
Input(X) into a 4-D tensor with shape [2, 3, 4, 2] and leaving Input(X)'s data
unchanged. In this case, one and only dimension of Attr(shape) can be set to -1,
the value of this dimension is inferred from the total element number of
Input(X) and remaining dimensions.

1. Given a 3-D tensor Input(X) with a shape [2, 4, 6], and the target shape
specified by Attr(shape) is [-1, 0, 3, 2], the reshape operator will transform
Input(X) into a 4-D tensor with shape [2, 4, 3, 2] and leaving Input(X)'s data
unchanged. In this case, besides -1, 0 means the actual dimension value is going
to be copied from the corresponding dimension of Input(X).

Note:

1. One and only one dimension in Attr(shape) can be set -1. In this case,
the actual dimension value will be infered from the total element number of
Input(X) and remaining dimensions.
1. More than one dimensions in Attr(shape) can be set to 0, which means the real
dimension value will be copied from Input(X) at runtime. Note that the index of
0 can not access Rank(X). For example, Input(X) is a 3-D tensor with shape
[2, 3, 4], Attr(shape) = [2, 3, 2, 0] is an invalid input.

)DOC");
  }
};

class ReshapeGradOp : public framework::OperatorWithKernel {
 public:
  ReshapeGradOp(const std::string &type,
                const framework::VariableNameMap &inputs,
                const framework::VariableNameMap &outputs,
                const framework::AttributeMap &attrs)
      : OperatorWithKernel(type, inputs, outputs, attrs) {}

  void InferShape(framework::InferShapeContext *ctx) const override {
    PADDLE_ENFORCE(ctx->HasInput("X"), "Input(X) shouldn't be null.");
    PADDLE_ENFORCE(ctx->HasInput(framework::GradVarName("Out")),
                   "Input(Out@GRAD) shouldn't be null.");
    ctx->SetOutputDim(framework::GradVarName("X"), ctx->GetInputDim("X"));
  }
};

}  // namespace operators
}  // namespace paddle
namespace ops = paddle::operators;
using CPU = paddle::platform::CPUDeviceContext;

REGISTER_OP(reshape, ops::ReshapeOp, ops::ReshapeOpMaker, reshape_grad,
            ops::ReshapeGradOp);
REGISTER_OP_CPU_KERNEL(reshape, ops::ReshapeKernel<CPU, float>,
                       ops::ReshapeKernel<CPU, double>,
                       ops::ReshapeKernel<CPU, int>,
                       ops::ReshapeKernel<CPU, int64_t>);
REGISTER_OP_CPU_KERNEL(reshape_grad, ops::ReshapeGradKernel<CPU, float>,
                       ops::ReshapeGradKernel<CPU, double>,
                       ops::ReshapeGradKernel<CPU, int>,
                       ops::ReshapeGradKernel<CPU, int64_t>);
