// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_SERIALIZATION_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/component_export.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "mojo/public/interfaces/bindings/native_struct.mojom.h"

namespace mojo {
namespace internal {

// Base class for the templated native struct serialization interface below,
// used to consolidated some shared logic and provide a basic
// Serialize/Deserialize for [Native] mojom structs which do not have a
// registered typemap in the current configuration (i.e. structs that are
// represented by a raw native::NativeStruct mojom struct in C++ bindings.)
struct COMPONENT_EXPORT(MOJO_CPP_BINDINGS) UnmappedNativeStructSerializerImpl {
  static void Serialize(
      const native::NativeStructPtr& input,
      Buffer* buffer,
      native::internal::NativeStruct_Data::BufferWriter* writer,
      SerializationContext* context);

  static bool Deserialize(native::internal::NativeStruct_Data* input,
                          native::NativeStructPtr* output,
                          SerializationContext* context);

};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_SERIALIZATION_H_
