/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PyDataConverter.h"
#include <vector>

#include "PyException.h"
#include "Types.h"
#include "utils/gsl.h"
#include "core/TypedValues.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PyDataConverter_methods[] = {
    {"timePeriodStringToMilliseconds", (PyCFunction) PyDataConverter::timePeriodStringToMilliseconds, METH_VARARGS, nullptr},
    {"dataSizeStringToBytes", (PyCFunction) PyDataConverter::dataSizeStringToBytes, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyDataConverterTypeSpecSlots[] = {
    {Py_tp_dealloc, reinterpret_cast<void*>(pythonAllocatedInstanceDealloc<PyDataConverter>)},
    {Py_tp_init, reinterpret_cast<void*>(PyDataConverter::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyDataConverter_methods)},
    {Py_tp_new, reinterpret_cast<void*>(newPythonAllocatedInstance<PyDataConverter>)},
    {}  /* Sentinel */
};

static PyType_Spec PyDataConverterTypeSpec{
    .name = "minifi_native.DataConverter",
    .basicsize = sizeof(PyDataConverter),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyDataConverterTypeSpecSlots
};

int PyDataConverter::init(PyDataConverter*, PyObject*, PyObject*) {
  return 0;
}

PyObject* PyDataConverter::timePeriodStringToMilliseconds(PyDataConverter*, PyObject* args) {
  const char* time_period_str = nullptr;
  if (!PyArg_ParseTuple(args, "s", &time_period_str)) {
    throw PyException();
  }

  auto milliseconds = core::TimePeriodValue(std::string(time_period_str)).getMilliseconds().count();

  return object::returnReference(milliseconds);
}

PyObject* PyDataConverter::dataSizeStringToBytes(PyDataConverter*, PyObject* args) {
  const char* data_size_str = nullptr;
  if (!PyArg_ParseTuple(args, "s", &data_size_str)) {
    throw PyException();
  }

  uint64_t bytes = core::DataSizeValue(std::string(data_size_str)).getValue();

  return object::returnReference(bytes);
}

PyTypeObject* PyDataConverter::typeObject() {
  static OwnedObject PyDataConverterType{PyType_FromSpec(&PyDataConverterTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyDataConverterType.get());
}

}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
