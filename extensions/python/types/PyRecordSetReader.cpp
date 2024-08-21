/**
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
a * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PyRecordSetReader.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PyRecordSetReader_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"read", (PyCFunction) PyRecordSetReader::read, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyRecordSetReaderTypeSpecSlots[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {Py_tp_dealloc, reinterpret_cast<void*>(pythonAllocatedInstanceDealloc<PyRecordSetReader>)},
    {Py_tp_init, reinterpret_cast<void*>(PyRecordSetReader::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyRecordSetReader_methods)},
    {Py_tp_new, reinterpret_cast<void*>(newPythonAllocatedInstance<PyRecordSetReader>)},
    {}  /* Sentinel */
};

static PyType_Spec PyRecordSetReaderTypeSpec{
    .name = "minifi_native.RecordSetReader",
    .basicsize = sizeof(PyRecordSetReader),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyRecordSetReaderTypeSpecSlots
};

int PyRecordSetReader::init(PyRecordSetReader* self, PyObject* args, PyObject*) {
  PyObject* weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto record_set_reader = PyCapsule_GetPointer(weak_ptr_capsule, HeldTypeName);
  if (!record_set_reader)
    return -1;
  self->record_set_reader_ = *static_cast<HeldType*>(record_set_reader);
  return 0;
}

PyObject* PyRecordSetReader::read(PyRecordSetReader* self, PyObject* /*args*/) {
  auto record_set_reader = self->record_set_reader_.lock();
  if (!record_set_reader) {
    PyErr_SetString(PyExc_AttributeError, "tried reading ssl context service outside 'on_trigger'");
    return nullptr;
  }
  // return object::returnReference(record_set_reader->read());
  return nullptr;
}

PyTypeObject* PyRecordSetReader::typeObject() {
  static OwnedObject PyRecordSetReaderType{PyType_FromSpec(&PyRecordSetReaderTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyRecordSetReaderType.get());
}

}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
