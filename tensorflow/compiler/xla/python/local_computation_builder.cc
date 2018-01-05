/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/xla/python/local_computation_builder.h"
#include "tensorflow/compiler/xla/executable_run_options.h"
#include "tensorflow/compiler/xla/util.h"

namespace xla {

namespace swig {

void TransferToInfeedLocal(const Literal& literal) {
  LocalClient* client = ClientLibrary::LocalClientOrDie();
  TF_CHECK_OK(client->TransferToInfeedLocal(literal, /*device_ordinal=*/0));
}

LocalShapedBuffer::LocalShapedBuffer(
    std::unique_ptr<ScopedShapedBuffer> shaped_buffer)
    : shaped_buffer_(std::move(shaped_buffer)) {}

const std::unique_ptr<ScopedShapedBuffer>& LocalShapedBuffer::shaped_buffer()
    const {
  return shaped_buffer_;
}

/* static */
LocalShapedBuffer* LocalShapedBuffer::FromLiteral(const Literal& argument) {
  LocalClient* client = ClientLibrary::LocalClientOrDie();
  std::unique_ptr<ScopedShapedBuffer> buf =
      client
          ->LiteralToShapedBuffer(argument,
                                  /*device_ordinal=*/0,
                                  client->backend().memory_allocator())
          .ConsumeValueOrDie();
  return new LocalShapedBuffer(std::move(buf));
}

std::unique_ptr<Literal> LocalShapedBuffer::ToLiteral() const {
  LocalClient* client = ClientLibrary::LocalClientOrDie();
  return client->ShapedBufferToLiteral(*shaped_buffer()).ConsumeValueOrDie();
}

CompiledLocalComputation::CompiledLocalComputation(
    std::unique_ptr<LocalExecutable> executable)
    : executable_(std::move(executable)) {}

std::unique_ptr<Literal> CompiledLocalComputation::Execute(
    const std::vector<Literal>& arguments) {
  LocalClient* client = ClientLibrary::LocalClientOrDie();

  // Transfer arguments in
  std::vector<std::unique_ptr<ScopedShapedBuffer>> scoped_buffers;
  scoped_buffers.reserve(arguments.size());
  for (const Literal& argument : arguments) {
    scoped_buffers.push_back(
        client
            ->LiteralToShapedBuffer(argument,
                                    /*device_ordinal=*/0,
                                    client->backend().memory_allocator())
            .ConsumeValueOrDie());
  }

  // Execute
  std::vector<const ShapedBuffer*> argument_buffers;
  argument_buffers.reserve(scoped_buffers.size());
  for (auto& buffer : scoped_buffers) {
    argument_buffers.push_back(buffer.get());
  }
  ExecutableRunOptions options;
  options.set_allocator(client->backend().memory_allocator());
  options.set_inter_op_thread_pool(client->backend().inter_op_thread_pool());
  options.set_intra_op_thread_pool(
      client->backend().eigen_intra_op_thread_pool_device());
  std::unique_ptr<ScopedShapedBuffer> result_buffer =
      executable_->Run(argument_buffers, options).ConsumeValueOrDie();

  // Transfer result out
  return client->ShapedBufferToLiteral(*result_buffer).ConsumeValueOrDie();
}

LocalShapedBuffer* CompiledLocalComputation::ExecuteWithShapedBuffers(
    tensorflow::gtl::ArraySlice<LocalShapedBuffer*> argument_handles) {
  LocalClient* client = ClientLibrary::LocalClientOrDie();

  std::vector<const ShapedBuffer*> argument_buffers;
  argument_buffers.reserve(argument_handles.size());
  for (auto& handle : argument_handles) {
    argument_buffers.push_back(handle->shaped_buffer().get());
  }

  // Execute
  ExecutableRunOptions options;
  options.set_allocator(client->backend().memory_allocator());
  options.set_inter_op_thread_pool(client->backend().inter_op_thread_pool());
  options.set_intra_op_thread_pool(
      client->backend().eigen_intra_op_thread_pool_device());
  std::unique_ptr<ScopedShapedBuffer> result_buffer =
      executable_->Run(argument_buffers, options).ConsumeValueOrDie();

  return new LocalShapedBuffer(std::move(result_buffer));
}

LocalComputation::LocalComputation(Computation computation)
    : computation_(std::move(computation)) {}

StatusOr<CompiledLocalComputation*> LocalComputation::Compile(
    const std::vector<Shape>& argument_shapes) {
  std::vector<const Shape*> argument_shape_pointers;
  argument_shape_pointers.reserve(argument_shapes.size());
  for (auto& argument_shape : argument_shapes) {
    argument_shape_pointers.push_back(&argument_shape);
  }

  LocalClient* client = ClientLibrary::LocalClientOrDie();
  ExecutableBuildOptions options;
  TF_ASSIGN_OR_RETURN(
      auto local_executable,
      client->Compile(computation_, argument_shape_pointers, options));
  return new CompiledLocalComputation(std::move(local_executable));
}

const Computation& LocalComputation::computation() const {
  return computation_;
}

LocalComputationBuilder::LocalComputationBuilder(const string& computation_name)
    : builder_(ClientLibrary::LocalClientOrDie(), computation_name) {}

StatusOr<LocalComputation*> LocalComputationBuilder::Build() {
  TF_ASSIGN_OR_RETURN(Computation computation, builder_.Build());
  return new LocalComputation(std::move(computation));
}

ComputationDataHandle LocalComputationBuilder::Parameter(int64 parameter_number,
                                                         const Shape& shape,
                                                         const string& name) {
  return builder_.Parameter(parameter_number, shape, name);
}

std::unique_ptr<Shape> LocalComputationBuilder::GetShape(
    const ComputationDataHandle& operand) {
  return builder_.GetShape(operand).ConsumeValueOrDie();
}

ComputationDataHandle LocalComputationBuilder::Infeed(const Shape& shape) {
  return builder_.Infeed(shape);
}

ComputationDataHandle LocalComputationBuilder::ConstantLiteral(
    const Literal& literal) {
  return builder_.ConstantLiteral(literal);
}

ComputationDataHandle LocalComputationBuilder::Broadcast(
    const ComputationDataHandle& operand,
    tensorflow::gtl::ArraySlice<int64> broadcast_sizes) {
  return builder_.Broadcast(operand, broadcast_sizes);
}

ComputationDataHandle LocalComputationBuilder::Reshape(
    const ComputationDataHandle& operand,
    tensorflow::gtl::ArraySlice<int64> dimensions,
    tensorflow::gtl::ArraySlice<int64> new_sizes) {
  return builder_.Reshape(operand, dimensions, new_sizes);
}

ComputationDataHandle LocalComputationBuilder::Collapse(
    const ComputationDataHandle& operand,
    tensorflow::gtl::ArraySlice<int64> dimensions) {
  return builder_.Collapse(operand, dimensions);
}

ComputationDataHandle LocalComputationBuilder::CrossReplicaSum(
    const ComputationDataHandle& operand) {
  return builder_.CrossReplicaSum(operand);
}

ComputationDataHandle LocalComputationBuilder::Slice(
    const ComputationDataHandle& operand,
    tensorflow::gtl::ArraySlice<int64> start_indices,
    tensorflow::gtl::ArraySlice<int64> limit_indices,
    tensorflow::gtl::ArraySlice<int64> strides) {
  return builder_.Slice(operand, start_indices, limit_indices, strides);
}

ComputationDataHandle LocalComputationBuilder::DynamicSlice(
    const ComputationDataHandle& operand,
    const ComputationDataHandle& start_indices,
    tensorflow::gtl::ArraySlice<int64> slice_sizes) {
  return builder_.DynamicSlice(operand, start_indices, slice_sizes);
}

ComputationDataHandle LocalComputationBuilder::DynamicUpdateSlice(
    const ComputationDataHandle& operand, const ComputationDataHandle& update,
    const ComputationDataHandle& start_indices) {
  return builder_.DynamicUpdateSlice(operand, update, start_indices);
}

ComputationDataHandle LocalComputationBuilder::ConcatInDim(
    tensorflow::gtl::ArraySlice<ComputationDataHandle> operands,
    int64 dimension) {
  return builder_.ConcatInDim(operands, dimension);
}

ComputationDataHandle LocalComputationBuilder::Select(
    const ComputationDataHandle& pred, const ComputationDataHandle& on_true,
    const ComputationDataHandle& on_false) {
  return builder_.Select(pred, on_true, on_false);
}

ComputationDataHandle LocalComputationBuilder::Tuple(
    tensorflow::gtl::ArraySlice<ComputationDataHandle> elements) {
  return builder_.Tuple(elements);
}

ComputationDataHandle LocalComputationBuilder::GetTupleElement(
    const ComputationDataHandle& tuple_data, int64 index) {
  return builder_.GetTupleElement(tuple_data, index);
}

ComputationDataHandle LocalComputationBuilder::Dot(
    const ComputationDataHandle& lhs, const ComputationDataHandle& rhs) {
  return builder_.Dot(lhs, rhs);
}

ComputationDataHandle LocalComputationBuilder::ConvGeneralDilated(
    const ComputationDataHandle& lhs, const ComputationDataHandle& rhs,
    tensorflow::gtl::ArraySlice<int64> window_strides,
    tensorflow::gtl::ArraySlice<std::pair<int64, int64>> padding,
    tensorflow::gtl::ArraySlice<int64> lhs_dilation,
    tensorflow::gtl::ArraySlice<int64> rhs_dilation,
    const ConvolutionDimensionNumbers& dimension_numbers) {
  return builder_.ConvGeneralDilated(lhs, rhs, window_strides, padding,
                                     lhs_dilation, rhs_dilation,
                                     dimension_numbers);
}

ComputationDataHandle LocalComputationBuilder::ConvertElementType(
    const ComputationDataHandle& operand, PrimitiveType new_element_type) {
  return builder_.ConvertElementType(operand, new_element_type);
}

ComputationDataHandle LocalComputationBuilder::Call(
    const LocalComputation& local_computation,
    tensorflow::gtl::ArraySlice<ComputationDataHandle> operands) {
  return builder_.Call(local_computation.computation(), operands);
}

ComputationDataHandle LocalComputationBuilder::Transpose(
    const ComputationDataHandle& operand,
    tensorflow::gtl::ArraySlice<int64> permutation) {
  return builder_.Transpose(operand, permutation);
}

ComputationDataHandle LocalComputationBuilder::Rev(
    const ComputationDataHandle& operand,
    tensorflow::gtl::ArraySlice<int64> dimensions) {
  return builder_.Rev(operand, dimensions);
}

ComputationDataHandle LocalComputationBuilder::Map(
    tensorflow::gtl::ArraySlice<ComputationDataHandle> operands,
    const LocalComputation& local_computation,
    tensorflow::gtl::ArraySlice<int64> dimensions,
    tensorflow::gtl::ArraySlice<ComputationDataHandle> static_operands) {
  return builder_.Map(operands, local_computation.computation(), dimensions,
                      static_operands);
}

ComputationDataHandle LocalComputationBuilder::Reduce(
    const ComputationDataHandle& operand,
    const ComputationDataHandle& init_value,
    const LocalComputation& local_computation,
    tensorflow::gtl::ArraySlice<int64> dimensions_to_reduce) {
  return builder_.Reduce(operand, init_value, local_computation.computation(),
                         dimensions_to_reduce);
}

ComputationDataHandle LocalComputationBuilder::RngNormal(
    const ComputationDataHandle& mu, const ComputationDataHandle& sigma,
    const Shape& shape) {
  return builder_.RngNormal(mu, sigma, shape);
}

ComputationDataHandle LocalComputationBuilder::RngUniform(
    const ComputationDataHandle& a, const ComputationDataHandle& b,
    const Shape& shape) {
  return builder_.RngUniform(a, b, shape);
}

ComputationDataHandle LocalComputationBuilder::While(
    const LocalComputation& condition, const LocalComputation& body,
    const ComputationDataHandle& init) {
  return builder_.While(condition.computation(), body.computation(), init);
}

#define _FORWARD(method_name, return_sig, args_sig, args)    \
  return_sig LocalComputationBuilder::method_name args_sig { \
    return builder_.method_name args;                        \
  }

#define _FORWARD_UNOP(method_name)             \
  _FORWARD(method_name, ComputationDataHandle, \
           (const ComputationDataHandle& operand), (operand))

#define _FORWARD_BINOP(method_name)                                        \
  _FORWARD(                                                                \
      method_name, ComputationDataHandle,                                  \
      (const ComputationDataHandle& lhs, const ComputationDataHandle& rhs, \
       tensorflow::gtl::ArraySlice<int64> broadcast_dimensions),           \
      (lhs, rhs, broadcast_dimensions))

_FORWARD_BINOP(Eq)
_FORWARD_BINOP(Ne)
_FORWARD_BINOP(Ge)
_FORWARD_BINOP(Gt)
_FORWARD_BINOP(Lt)
_FORWARD_BINOP(Le)
_FORWARD_BINOP(Add)
_FORWARD_BINOP(Sub)
_FORWARD_BINOP(Mul)
_FORWARD_BINOP(Div)
_FORWARD_BINOP(Rem)
_FORWARD_BINOP(Max)
_FORWARD_BINOP(Min)
_FORWARD_BINOP(And)
_FORWARD_BINOP(Or)
_FORWARD_UNOP(Not)
_FORWARD_UNOP(Abs)
_FORWARD_UNOP(Exp)
_FORWARD_UNOP(Floor)
_FORWARD_UNOP(Ceil)
_FORWARD_UNOP(Log)
_FORWARD_UNOP(Sign)
_FORWARD_UNOP(Cos)
_FORWARD_UNOP(Sin)
_FORWARD_UNOP(Tanh)
_FORWARD_UNOP(SqrtF32)
_FORWARD_UNOP(SquareF32)
_FORWARD_BINOP(Pow)
_FORWARD_UNOP(IsFinite)
_FORWARD_UNOP(ReciprocalF32)
_FORWARD_UNOP(Neg)
_FORWARD_UNOP(Sort)

#undef _FORWARD
#undef _FORWARD_UNOP
#undef _FORWARD_BINOP

void DeleteLocalShapedBuffer(LocalShapedBuffer* local_shaped_buffer) {
  delete local_shaped_buffer;
}

void DeleteCompiledLocalComputation(CompiledLocalComputation* computation) {
  delete computation;
}

void DeleteLocalComputation(LocalComputation* computation) {
  delete computation;
}

}  // namespace swig

}  // namespace xla