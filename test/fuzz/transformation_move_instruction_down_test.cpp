// Copyright (c) 2020 Vasyl Teliman
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "source/fuzz/transformation_move_instruction_down.h"

#include "source/fuzz/instruction_descriptor.h"
#include "test/fuzz/fuzz_test_util.h"

namespace spvtools {
namespace fuzz {
namespace {

TEST(TransformationMoveInstructionDownTest, BasicTest) {
  std::string shader = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %4 "main"
               OpExecutionMode %4 OriginUpperLeft
               OpSource ESSL 310
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeInt 32 1
          %9 = OpConstant %6 0
         %16 = OpTypeBool
         %17 = OpConstantFalse %16
         %20 = OpUndef %6
         %13 = OpTypePointer Function %6
          %4 = OpFunction %2 None %3
          %5 = OpLabel
         %12 = OpVariable %13 Function
         %10 = OpIAdd %6 %9 %9
         %11 = OpISub %6 %9 %10
               OpStore %12 %10
         %14 = OpLoad %6 %12
         %15 = OpIMul %6 %9 %14
               OpSelectionMerge %19 None
               OpBranchConditional %17 %18 %19
         %18 = OpLabel
               OpBranch %19
         %19 = OpLabel
         %42 = OpFunctionCall %2 %40
         %22 = OpIAdd %6 %15 %15
         %21 = OpIAdd %6 %15 %15
               OpReturn
               OpFunctionEnd
         %40 = OpFunction %2 None %3
         %41 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  const auto env = SPV_ENV_UNIVERSAL_1_3;
  const auto consumer = nullptr;
  const auto context = BuildModule(env, consumer, shader, kFuzzAssembleOption);
  ASSERT_TRUE(IsValid(env, context.get()));

  FactManager fact_manager(context.get());
  spvtools::ValidatorOptions validator_options;
  TransformationContext transformation_context(&fact_manager,
                                               validator_options);

  // Instruction descriptor is invalid.
  ASSERT_FALSE(TransformationMoveInstructionDown(
                   MakeInstructionDescriptor(30, SpvOpNop, 0))
                   .IsApplicable(context.get(), transformation_context));

  // Opcode is not supported.
  ASSERT_FALSE(TransformationMoveInstructionDown(
                   MakeInstructionDescriptor(5, SpvOpLabel, 0))
                   .IsApplicable(context.get(), transformation_context));
  ASSERT_FALSE(TransformationMoveInstructionDown(
                   MakeInstructionDescriptor(12, SpvOpVariable, 0))
                   .IsApplicable(context.get(), transformation_context));
  ASSERT_FALSE(TransformationMoveInstructionDown(
                   MakeInstructionDescriptor(42, SpvOpFunctionCall, 0))
                   .IsApplicable(context.get(), transformation_context));

  // Can't move the last instruction in the block.
  ASSERT_FALSE(TransformationMoveInstructionDown(
                   MakeInstructionDescriptor(15, SpvOpBranchConditional, 0))
                   .IsApplicable(context.get(), transformation_context));

  // Can't move the instruction if the next instruction is the last one in the
  // block.
  ASSERT_FALSE(TransformationMoveInstructionDown(
                   MakeInstructionDescriptor(21, SpvOpIAdd, 0))
                   .IsApplicable(context.get(), transformation_context));

  // Can't insert instruction's opcode after its successor.
  ASSERT_FALSE(TransformationMoveInstructionDown(
                   MakeInstructionDescriptor(15, SpvOpIMul, 0))
                   .IsApplicable(context.get(), transformation_context));

  // Instruction's successor depends on the instruction.
  ASSERT_FALSE(TransformationMoveInstructionDown(
                   MakeInstructionDescriptor(10, SpvOpIAdd, 0))
                   .IsApplicable(context.get(), transformation_context));

  {
    TransformationMoveInstructionDown transformation(
        MakeInstructionDescriptor(11, SpvOpISub, 0));
    ASSERT_TRUE(
        transformation.IsApplicable(context.get(), transformation_context));
    transformation.Apply(context.get(), &transformation_context);
    ASSERT_TRUE(IsValid(env, context.get()));
  }
  {
    TransformationMoveInstructionDown transformation(
        MakeInstructionDescriptor(22, SpvOpIAdd, 0));
    ASSERT_TRUE(
        transformation.IsApplicable(context.get(), transformation_context));
    transformation.Apply(context.get(), &transformation_context);
    ASSERT_TRUE(IsValid(env, context.get()));
  }

  std::string after_transformation = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %4 "main"
               OpExecutionMode %4 OriginUpperLeft
               OpSource ESSL 310
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeInt 32 1
          %9 = OpConstant %6 0
         %16 = OpTypeBool
         %17 = OpConstantFalse %16
         %20 = OpUndef %6
         %13 = OpTypePointer Function %6
          %4 = OpFunction %2 None %3
          %5 = OpLabel
         %12 = OpVariable %13 Function
         %10 = OpIAdd %6 %9 %9
               OpStore %12 %10
         %11 = OpISub %6 %9 %10
         %14 = OpLoad %6 %12
         %15 = OpIMul %6 %9 %14
               OpSelectionMerge %19 None
               OpBranchConditional %17 %18 %19
         %18 = OpLabel
               OpBranch %19
         %19 = OpLabel
         %42 = OpFunctionCall %2 %40
         %21 = OpIAdd %6 %15 %15
         %22 = OpIAdd %6 %15 %15
               OpReturn
               OpFunctionEnd
         %40 = OpFunction %2 None %3
         %41 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  ASSERT_TRUE(IsEqual(env, after_transformation, context.get()));
}

TEST(TransformationMoveInstructionDownTest, HandlesUnsupportedInstructions) {
  std::string shader = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %4 "main"
               OpExecutionMode %4 LocalSize 16 1 1
               OpSource ESSL 320
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeInt 32 0
          %7 = OpConstant %6 2
         %20 = OpTypePointer Function %6
          %4 = OpFunction %2 None %3
          %5 = OpLabel
         %21 = OpVariable %20 Function %7

          ; can swap simple and not supported instructions
          %8 = OpCopyObject %6 %7
          %9 = OpFunctionCall %2 %12

         ; cannot swap memory and not supported instruction
         %22 = OpLoad %6 %21
         %23 = OpFunctionCall %2 %12

         ; cannot swap barrier and not supported instruction
               OpMemoryBarrier %7 %7
         %24 = OpFunctionCall %2 %12

               OpReturn
               OpFunctionEnd
         %12 = OpFunction %2 None %3
         %13 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  const auto env = SPV_ENV_UNIVERSAL_1_3;
  const auto consumer = nullptr;
  const auto context = BuildModule(env, consumer, shader, kFuzzAssembleOption);
  ASSERT_TRUE(IsValid(env, context.get()));

  FactManager fact_manager(context.get());
  spvtools::ValidatorOptions validator_options;
  TransformationContext transformation_context(&fact_manager,
                                               validator_options);

  // Swap memory instruction with an unsupported one.
  ASSERT_FALSE(TransformationMoveInstructionDown(
                   MakeInstructionDescriptor(22, SpvOpLoad, 0))
                   .IsApplicable(context.get(), transformation_context));

  // Swap memory barrier with an unsupported one.
  ASSERT_FALSE(TransformationMoveInstructionDown(
                   MakeInstructionDescriptor(23, SpvOpMemoryBarrier, 0))
                   .IsApplicable(context.get(), transformation_context));

  // Swap simple instruction with an unsupported one.
  TransformationMoveInstructionDown transformation(
      MakeInstructionDescriptor(8, SpvOpCopyObject, 0));
  ASSERT_TRUE(
      transformation.IsApplicable(context.get(), transformation_context));
  transformation.Apply(context.get(), &transformation_context);
  ASSERT_TRUE(IsValid(env, context.get()));

  std::string after_transformation = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %4 "main"
               OpExecutionMode %4 LocalSize 16 1 1
               OpSource ESSL 320
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeInt 32 0
          %7 = OpConstant %6 2
         %20 = OpTypePointer Function %6
          %4 = OpFunction %2 None %3
          %5 = OpLabel
         %21 = OpVariable %20 Function %7

          ; can swap simple and not supported instructions
          %9 = OpFunctionCall %2 %12
          %8 = OpCopyObject %6 %7

         ; cannot swap memory and not supported instruction
         %22 = OpLoad %6 %21
         %23 = OpFunctionCall %2 %12

         ; cannot swap barrier and not supported instruction
               OpMemoryBarrier %7 %7
         %24 = OpFunctionCall %2 %12

               OpReturn
               OpFunctionEnd
         %12 = OpFunction %2 None %3
         %13 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  ASSERT_TRUE(IsEqual(env, after_transformation, context.get()));
}

TEST(TransformationMoveInstructionDownTest, HandlesBarrierInstructions) {
  std::string shader = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %4 "main"
               OpExecutionMode %4 LocalSize 16 1 1
               OpSource ESSL 320
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeInt 32 0
          %7 = OpConstant %6 2
         %20 = OpTypePointer Function %6
          %4 = OpFunction %2 None %3
          %5 = OpLabel
         %21 = OpVariable %20 Function %7

          ; cannot swap two barrier instructions
               OpMemoryBarrier %7 %7
               OpMemoryBarrier %7 %7

         ; cannot swap barrier and memory instructions
               OpMemoryBarrier %7 %7
         %22 = OpLoad %6 %21
               OpMemoryBarrier %7 %7

         ; can swap barrier and simple instructions
         %23 = OpCopyObject %6 %7
               OpMemoryBarrier %7 %7

               OpReturn
               OpFunctionEnd
         %12 = OpFunction %2 None %3
         %13 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  const auto env = SPV_ENV_UNIVERSAL_1_3;
  const auto consumer = nullptr;
  const auto context = BuildModule(env, consumer, shader, kFuzzAssembleOption);
  ASSERT_TRUE(IsValid(env, context.get()));

  FactManager fact_manager(context.get());
  spvtools::ValidatorOptions validator_options;
  TransformationContext transformation_context(&fact_manager,
                                               validator_options);

  // Swap two barrier instructions.
  ASSERT_FALSE(TransformationMoveInstructionDown(
                   MakeInstructionDescriptor(21, SpvOpMemoryBarrier, 0))
                   .IsApplicable(context.get(), transformation_context));

  // Swap barrier and memory instructions.
  ASSERT_FALSE(TransformationMoveInstructionDown(
                   MakeInstructionDescriptor(21, SpvOpMemoryBarrier, 2))
                   .IsApplicable(context.get(), transformation_context));
  ASSERT_FALSE(TransformationMoveInstructionDown(
                   MakeInstructionDescriptor(22, SpvOpLoad, 0))
                   .IsApplicable(context.get(), transformation_context));

  // Swap barrier and simple instructions.
  {
    TransformationMoveInstructionDown transformation(
        MakeInstructionDescriptor(23, SpvOpCopyObject, 0));
    ASSERT_TRUE(
        transformation.IsApplicable(context.get(), transformation_context));
    transformation.Apply(context.get(), &transformation_context);
    ASSERT_TRUE(IsValid(env, context.get()));
  }
  {
    TransformationMoveInstructionDown transformation(
        MakeInstructionDescriptor(22, SpvOpMemoryBarrier, 1));
    ASSERT_TRUE(
        transformation.IsApplicable(context.get(), transformation_context));
    transformation.Apply(context.get(), &transformation_context);
    ASSERT_TRUE(IsValid(env, context.get()));
  }

  ASSERT_TRUE(IsEqual(env, shader, context.get()));
}

TEST(TransformationMoveInstructionDownTest, HandlesSimpleInstructions) {
  std::string shader = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %4 "main"
               OpExecutionMode %4 LocalSize 16 1 1
               OpSource ESSL 320
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeInt 32 0
          %7 = OpConstant %6 2
         %20 = OpTypePointer Function %6
          %4 = OpFunction %2 None %3
          %5 = OpLabel
         %21 = OpVariable %20 Function %7

         ; can swap simple and barrier instructions
         %40 = OpCopyObject %6 %7
               OpMemoryBarrier %7 %7

         ; can swap simple and memory instructions
         %41 = OpCopyObject %6 %7
         %22 = OpLoad %6 %21

         ; can swap two simple instructions
         %23 = OpCopyObject %6 %7
         %42 = OpCopyObject %6 %7

               OpReturn
               OpFunctionEnd
         %12 = OpFunction %2 None %3
         %13 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  const auto env = SPV_ENV_UNIVERSAL_1_3;
  const auto consumer = nullptr;
  const auto context = BuildModule(env, consumer, shader, kFuzzAssembleOption);
  ASSERT_TRUE(IsValid(env, context.get()));

  FactManager fact_manager(context.get());
  spvtools::ValidatorOptions validator_options;
  TransformationContext transformation_context(&fact_manager,
                                               validator_options);

  // Swap simple and barrier instructions.
  {
    TransformationMoveInstructionDown transformation(
        MakeInstructionDescriptor(40, SpvOpCopyObject, 0));
    ASSERT_TRUE(
        transformation.IsApplicable(context.get(), transformation_context));
    transformation.Apply(context.get(), &transformation_context);
    ASSERT_TRUE(IsValid(env, context.get()));
  }
  {
    TransformationMoveInstructionDown transformation(
        MakeInstructionDescriptor(21, SpvOpMemoryBarrier, 0));
    ASSERT_TRUE(
        transformation.IsApplicable(context.get(), transformation_context));
    transformation.Apply(context.get(), &transformation_context);
    ASSERT_TRUE(IsValid(env, context.get()));
  }

  // Swap simple and memory instructions.
  {
    TransformationMoveInstructionDown transformation(
        MakeInstructionDescriptor(41, SpvOpCopyObject, 0));
    ASSERT_TRUE(
        transformation.IsApplicable(context.get(), transformation_context));
    transformation.Apply(context.get(), &transformation_context);
    ASSERT_TRUE(IsValid(env, context.get()));
  }
  {
    TransformationMoveInstructionDown transformation(
        MakeInstructionDescriptor(22, SpvOpLoad, 0));
    ASSERT_TRUE(
        transformation.IsApplicable(context.get(), transformation_context));
    transformation.Apply(context.get(), &transformation_context);
    ASSERT_TRUE(IsValid(env, context.get()));
  }

  // Swap two simple instructions.
  {
    TransformationMoveInstructionDown transformation(
        MakeInstructionDescriptor(23, SpvOpCopyObject, 0));
    ASSERT_TRUE(
        transformation.IsApplicable(context.get(), transformation_context));
    transformation.Apply(context.get(), &transformation_context);
    ASSERT_TRUE(IsValid(env, context.get()));
  }

  std::string after_transformation = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %4 "main"
               OpExecutionMode %4 LocalSize 16 1 1
               OpSource ESSL 320
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeInt 32 0
          %7 = OpConstant %6 2
         %20 = OpTypePointer Function %6
          %4 = OpFunction %2 None %3
          %5 = OpLabel
         %21 = OpVariable %20 Function %7

         ; can swap simple and barrier instructions
         %40 = OpCopyObject %6 %7
               OpMemoryBarrier %7 %7

         ; can swap simple and memory instructions
         %41 = OpCopyObject %6 %7
         %22 = OpLoad %6 %21

         ; can swap two simple instructions
         %42 = OpCopyObject %6 %7
         %23 = OpCopyObject %6 %7

               OpReturn
               OpFunctionEnd
         %12 = OpFunction %2 None %3
         %13 = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  ASSERT_TRUE(IsEqual(env, after_transformation, context.get()));
}

TEST(TransformationMoveInstructionDownTest, HandlesMemoryInstructions) {
  std::string shader = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %4 "main"
               OpExecutionMode %4 LocalSize 16 1 1
               OpSource ESSL 320
          %2 = OpTypeVoid
          %3 = OpTypeFunction %2
          %6 = OpTypeInt 32 0
          %7 = OpConstant %6 2
         %20 = OpTypePointer Function %6
          %4 = OpFunction %2 None %3
          %5 = OpLabel
         %21 = OpVariable %20 Function %7
         %22 = OpVariable %20 Function %7

         ; swap R and R instructions
         %23 = OpLoad %6 %21
         %24 = OpLoad %6 %22

         ; swap R and RW instructions

           ; can't swap
         %25 = OpLoad %6 %21
               OpCopyMemory %21 %22

           ; can swap
         %26 = OpLoad %6 %21
               OpCopyMemory %22 %21

         %27 = OpLoad %6 %22
               OpCopyMemory %21 %22

         %28 = OpLoad %6 %22
               OpCopyMemory %22 %21

         ; swap R and W instructions

           ; can't swap
         %29 = OpLoad %6 %21
               OpStore %21 %7

           ; can swap
         %30 = OpLoad %6 %22
               OpStore %21 %7

         %31 = OpLoad %6 %21
               OpStore %22 %7

         %32 = OpLoad %6 %22
               OpStore %22 %7

         ; swap RW and RW instructions

           ; can't swap
               OpCopyMemory %21 %21
               OpCopyMemory %21 %21

               OpCopyMemory %21 %22
               OpCopyMemory %21 %21

               OpCopyMemory %21 %21
               OpCopyMemory %21 %22

           ; can swap
               OpCopyMemory %22 %21
               OpCopyMemory %21 %22

               OpCopyMemory %22 %21
               OpCopyMemory %22 %21

               OpCopyMemory %21 %22
               OpCopyMemory %21 %22

         ; swap RW and W instructions

           ; can't swap
               OpCopyMemory %21 %21
               OpStore %21 %7

               OpStore %21 %7
               OpCopyMemory %21 %21

           ; can swap
               OpCopyMemory %22 %21
               OpStore %21 %7

               OpCopyMemory %21 %22
               OpStore %21 %7

               OpCopyMemory %21 %21
               OpStore %22 %7

         ; swap W and W instructions

           ; can't swap
               OpStore %21 %7
               OpStore %21 %7

           ; can swap
               OpStore %22 %7
               OpStore %21 %7

               OpStore %22 %7
               OpStore %22 %7

               OpReturn
               OpFunctionEnd
  )";

  const auto env = SPV_ENV_UNIVERSAL_1_3;
  const auto consumer = nullptr;
  const auto context = BuildModule(env, consumer, shader, kFuzzAssembleOption);
  ASSERT_TRUE(IsValid(env, context.get()));

  FactManager fact_manager(context.get());
  spvtools::ValidatorOptions validator_options;
  TransformationContext transformation_context(&fact_manager,
                                               validator_options);

  fact_manager.AddFactValueOfPointeeIsIrrelevant(22);

  // Invalid swaps.

  protobufs::InstructionDescriptor invalid_swaps[] = {
      // R and RW
      MakeInstructionDescriptor(25, SpvOpLoad, 0),

      // R and W
      MakeInstructionDescriptor(29, SpvOpLoad, 0),

      // RW and RW
      MakeInstructionDescriptor(32, SpvOpCopyMemory, 0),
      MakeInstructionDescriptor(32, SpvOpCopyMemory, 2),
      MakeInstructionDescriptor(32, SpvOpCopyMemory, 4),

      // RW and W
      MakeInstructionDescriptor(32, SpvOpCopyMemory, 12),
      MakeInstructionDescriptor(32, SpvOpStore, 1),

      // W and W
      MakeInstructionDescriptor(32, SpvOpStore, 6),
  };

  for (const auto& descriptor : invalid_swaps) {
    ASSERT_FALSE(TransformationMoveInstructionDown(descriptor)
                     .IsApplicable(context.get(), transformation_context));
  }

  // Valid swaps.
  protobufs::InstructionDescriptor valid_swaps[] = {
      // R and R
      MakeInstructionDescriptor(23, SpvOpLoad, 0),
      MakeInstructionDescriptor(24, SpvOpLoad, 0),

      // R and RW
      MakeInstructionDescriptor(26, SpvOpLoad, 0),
      MakeInstructionDescriptor(25, SpvOpCopyMemory, 1),

      MakeInstructionDescriptor(27, SpvOpLoad, 0),
      MakeInstructionDescriptor(26, SpvOpCopyMemory, 1),

      MakeInstructionDescriptor(28, SpvOpLoad, 0),
      MakeInstructionDescriptor(27, SpvOpCopyMemory, 1),

      // R and W
      MakeInstructionDescriptor(30, SpvOpLoad, 0),
      MakeInstructionDescriptor(29, SpvOpStore, 1),

      MakeInstructionDescriptor(31, SpvOpLoad, 0),
      MakeInstructionDescriptor(30, SpvOpStore, 1),

      MakeInstructionDescriptor(32, SpvOpLoad, 0),
      MakeInstructionDescriptor(31, SpvOpStore, 1),

      // RW and RW
      MakeInstructionDescriptor(32, SpvOpCopyMemory, 6),
      MakeInstructionDescriptor(32, SpvOpCopyMemory, 6),

      MakeInstructionDescriptor(32, SpvOpCopyMemory, 8),
      MakeInstructionDescriptor(32, SpvOpCopyMemory, 8),

      MakeInstructionDescriptor(32, SpvOpCopyMemory, 10),
      MakeInstructionDescriptor(32, SpvOpCopyMemory, 10),

      // RW and W
      MakeInstructionDescriptor(32, SpvOpCopyMemory, 14),
      MakeInstructionDescriptor(32, SpvOpStore, 3),

      MakeInstructionDescriptor(32, SpvOpCopyMemory, 15),
      MakeInstructionDescriptor(32, SpvOpStore, 4),

      MakeInstructionDescriptor(32, SpvOpCopyMemory, 16),
      MakeInstructionDescriptor(32, SpvOpStore, 5),

      // W and W
      MakeInstructionDescriptor(32, SpvOpStore, 8),
      MakeInstructionDescriptor(32, SpvOpStore, 8),

      MakeInstructionDescriptor(32, SpvOpStore, 10),
      MakeInstructionDescriptor(32, SpvOpStore, 10),
  };

  for (const auto& descriptor : valid_swaps) {
    TransformationMoveInstructionDown transformation(descriptor);
    ASSERT_TRUE(
        transformation.IsApplicable(context.get(), transformation_context));
    transformation.Apply(context.get(), &transformation_context);
    ASSERT_TRUE(IsValid(env, context.get()));
  }

  ASSERT_TRUE(IsEqual(env, shader, context.get()));
}

}  // namespace
}  // namespace fuzz
}  // namespace spvtools
