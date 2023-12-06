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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PythonBindings.h"
#include <utility>
#include "types/PyLogger.h"
#include "types/PyProcessSession.h"
#include "types/PyProcessContext.h"
#include "types/PyProcessor.h"
#include "types/PyScriptFlowFile.h"
#include "types/PyRelationship.h"
#include "types/PyInputStream.h"
#include "types/PyOutputStream.h"
#include "types/PyStateManager.h"

namespace minifi_python = org::apache::nifi::minifi::extensions::python;

static struct PyModuleDef minifi_module = {
  .m_base = PyModuleDef_HEAD_INIT,
  .m_name = "minifi_native",    // name of module
  .m_doc = "Example",             // module documentation, may be NULL
  .m_size = -1,                 // size of per-interpreter state of the module, or -1 if the module keeps state in global variables.
  .m_methods = nullptr,
  .m_slots = nullptr,
  .m_traverse = nullptr,
  .m_clear = nullptr,
  .m_free = nullptr
};

PyMODINIT_FUNC PyInit_minifi_native(void) {
  PyObject* minifi_module_instance;
  const std::array types = std::to_array<std::pair<PyTypeObject*, std::string_view>>({
      std::make_pair(minifi_python::PyLogger::typeObject(), "Logger"),
      std::make_pair(minifi_python::PyProcessSessionObject::typeObject(), "ProcessSession"),
      std::make_pair(minifi_python::PyProcessContext::typeObject(), "ProcessContext"),
      std::make_pair(minifi_python::PyProcessor::typeObject(), "Processor"),
      std::make_pair(minifi_python::PyScriptFlowFile::typeObject(), "FlowFile"),
      std::make_pair(minifi_python::PyRelationship::typeObject(), "Relationship"),
      std::make_pair(minifi_python::PyInputStream::typeObject(), "InputStream"),
      std::make_pair(minifi_python::PyOutputStream::typeObject(), "OutputStream"),
      std::make_pair(minifi_python::PyStateManager::typeObject(), "StateManager")
  });

  for (const auto& type : types) {
    if (PyType_Ready(type.first) < 0) {
      return nullptr;
    }
  }

  minifi_module_instance = PyModule_Create(&minifi_module);
  if (minifi_module_instance == nullptr) {
      return nullptr;
  }

  for (const auto& type : types) {
    Py_INCREF(type.first);
  }
  const auto result = std::all_of(std::begin(types), std::end(types), [&](std::pair<PyTypeObject*, std::string_view> type) {
    return PyModule_AddObject(minifi_module_instance, type.second.data(), reinterpret_cast<PyObject*>(type.first)) == 0;
  });

  if (!result) {
    for (const auto& type : types) {
      Py_DECREF(type.first);
    }
    Py_DECREF(minifi_module_instance);
    return nullptr;
  }

  return minifi_module_instance;
}
