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

/*
 * OPC UA test server exposing the files of a directory through the FileType and FileDirectoryType object types.
 *
 * The "Files" FileDirectoryType node (ns=1;s=Files) contains a FileType node (ns=1;s=<file name>) for every regular file
 * found in the directory at startup. Files created with the CreateFile method get a node id assigned by the server.
 * The FileType and FileDirectoryType methods only define an interface, so their behavior is implemented here.
 */

#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_OPEN_HANDLES 16

/* Open mode bits of the FileType Open method (OPC UA Part 20, 4.2.2) */
#define FILE_MODE_READ 0x01
#define FILE_MODE_WRITE 0x02
#define FILE_MODE_ERASE_EXISTING 0x04
#define FILE_MODE_APPEND 0x08

typedef struct {
    UA_UInt32 id;
    UA_Byte mode;
    FILE *file;
} OpenHandle;

typedef struct FileContext {
    char path[PATH_MAX];
    OpenHandle handles[MAX_OPEN_HANDLES];
    struct FileContext *next;
} FileContext;

static const char *data_dir;
static const UA_Logger *logger;
static FileContext *file_contexts;
static UA_UInt32 next_handle_id = 1;
static const UA_NodeId files_node_id = {1, UA_NODEIDTYPE_STRING, {.string = {5, (UA_Byte *)"Files"}}};

static OpenHandle *
findHandle(FileContext *context, UA_UInt32 handle_id) {
    for(size_t i = 0; i < MAX_OPEN_HANDLES; ++i) {
        if(context->handles[i].file && context->handles[i].id == handle_id)
            return &context->handles[i];
    }
    return NULL;
}

static UA_StatusCode
openHandle(FileContext *context, UA_Byte mode, UA_UInt32 *handle_id) {
    const char *fopen_mode;
    if(mode & FILE_MODE_WRITE) {
        if(access(context->path, W_OK) != 0)
            return UA_STATUSCODE_BADNOTWRITABLE;
        if(mode & FILE_MODE_ERASE_EXISTING)
            fopen_mode = (mode & FILE_MODE_READ) ? "w+b" : "wb";
        else if(mode & FILE_MODE_APPEND)
            fopen_mode = (mode & FILE_MODE_READ) ? "a+b" : "ab";
        else
            fopen_mode = "r+b";
    } else if(mode & FILE_MODE_READ) {
        fopen_mode = "rb";
    } else {
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    }

    OpenHandle *handle = NULL;
    for(size_t i = 0; i < MAX_OPEN_HANDLES && !handle; ++i) {
        if(!context->handles[i].file)
            handle = &context->handles[i];
    }
    if(!handle)
        return UA_STATUSCODE_BADTOOMANYOPERATIONS;

    FILE *file = fopen(context->path, fopen_mode);
    if(!file)
        return UA_STATUSCODE_BADINTERNALERROR;
    handle->id = next_handle_id++;
    handle->mode = mode;
    handle->file = file;
    *handle_id = handle->id;
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
openMethod(UA_Server *server, const UA_NodeId *session_id, void *session_context, const UA_NodeId *method_id, void *method_context,
           const UA_NodeId *object_id, void *object_context, size_t input_size, const UA_Variant *input, size_t output_size,
           UA_Variant *output) {
    FileContext *context = (FileContext *)object_context;
    if(!context)
        return UA_STATUSCODE_BADNOTFOUND;
    const UA_Byte mode = *(const UA_Byte *)input[0].data;
    UA_UInt32 handle_id = 0;
    const UA_StatusCode status = openHandle(context, mode, &handle_id);
    UA_LOG_INFO(logger, UA_LOGCATEGORY_USERLAND, "Open %s with mode %u: %s, handle %u", context->path, (unsigned)mode,
                UA_StatusCode_name(status), (unsigned)handle_id);
    if(status != UA_STATUSCODE_GOOD)
        return status;
    return UA_Variant_setScalarCopy(&output[0], &handle_id, &UA_TYPES[UA_TYPES_UINT32]);
}

static UA_StatusCode
closeMethod(UA_Server *server, const UA_NodeId *session_id, void *session_context, const UA_NodeId *method_id, void *method_context,
            const UA_NodeId *object_id, void *object_context, size_t input_size, const UA_Variant *input, size_t output_size,
            UA_Variant *output) {
    FileContext *context = (FileContext *)object_context;
    if(!context)
        return UA_STATUSCODE_BADNOTFOUND;
    OpenHandle *handle = findHandle(context, *(const UA_UInt32 *)input[0].data);
    if(!handle)
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    const int result = fclose(handle->file);
    UA_LOG_INFO(logger, UA_LOGCATEGORY_USERLAND, "Close %s handle %u", context->path, (unsigned)handle->id);
    handle->file = NULL;
    handle->id = 0;
    return result == 0 ? UA_STATUSCODE_GOOD : UA_STATUSCODE_BADINTERNALERROR;
}

static UA_StatusCode
readMethod(UA_Server *server, const UA_NodeId *session_id, void *session_context, const UA_NodeId *method_id, void *method_context,
           const UA_NodeId *object_id, void *object_context, size_t input_size, const UA_Variant *input, size_t output_size,
           UA_Variant *output) {
    FileContext *context = (FileContext *)object_context;
    if(!context)
        return UA_STATUSCODE_BADNOTFOUND;
    OpenHandle *handle = findHandle(context, *(const UA_UInt32 *)input[0].data);
    const UA_Int32 length = *(const UA_Int32 *)input[1].data;
    if(!handle || !(handle->mode & FILE_MODE_READ))
        return UA_STATUSCODE_BADINVALIDSTATE;
    if(length < 0)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    UA_ByteString data = UA_BYTESTRING_NULL;
    if(length > 0) {
        UA_StatusCode status = UA_ByteString_allocBuffer(&data, (size_t)length);
        if(status != UA_STATUSCODE_GOOD)
            return status;
        data.length = fread(data.data, 1, (size_t)length, handle->file);
        if(ferror(handle->file)) {
            UA_ByteString_clear(&data);
            return UA_STATUSCODE_BADINTERNALERROR;
        }
    }
    UA_LOG_INFO(logger, UA_LOGCATEGORY_USERLAND, "Read %lu bytes from %s", (unsigned long)data.length, context->path);
    UA_StatusCode status = UA_Variant_setScalarCopy(&output[0], &data, &UA_TYPES[UA_TYPES_BYTESTRING]);
    UA_ByteString_clear(&data);
    return status;
}

static UA_StatusCode
writeMethod(UA_Server *server, const UA_NodeId *session_id, void *session_context, const UA_NodeId *method_id, void *method_context,
            const UA_NodeId *object_id, void *object_context, size_t input_size, const UA_Variant *input, size_t output_size,
            UA_Variant *output) {
    FileContext *context = (FileContext *)object_context;
    if(!context)
        return UA_STATUSCODE_BADNOTFOUND;
    OpenHandle *handle = findHandle(context, *(const UA_UInt32 *)input[0].data);
    const UA_ByteString *data = (const UA_ByteString *)input[1].data;
    if(!handle || !(handle->mode & FILE_MODE_WRITE))
        return UA_STATUSCODE_BADINVALIDSTATE;
    if(data->length > 0 && fwrite(data->data, 1, data->length, handle->file) != data->length)
        return UA_STATUSCODE_BADINTERNALERROR;
    UA_LOG_INFO(logger, UA_LOGCATEGORY_USERLAND, "Wrote %lu bytes to %s", (unsigned long)data->length, context->path);
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
getPositionMethod(UA_Server *server, const UA_NodeId *session_id, void *session_context, const UA_NodeId *method_id, void *method_context,
                  const UA_NodeId *object_id, void *object_context, size_t input_size, const UA_Variant *input, size_t output_size,
                  UA_Variant *output) {
    FileContext *context = (FileContext *)object_context;
    if(!context)
        return UA_STATUSCODE_BADNOTFOUND;
    OpenHandle *handle = findHandle(context, *(const UA_UInt32 *)input[0].data);
    if(!handle)
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    const long position = ftell(handle->file);
    if(position < 0)
        return UA_STATUSCODE_BADINTERNALERROR;
    const UA_UInt64 result = (UA_UInt64)position;
    return UA_Variant_setScalarCopy(&output[0], &result, &UA_TYPES[UA_TYPES_UINT64]);
}

static UA_StatusCode
setPositionMethod(UA_Server *server, const UA_NodeId *session_id, void *session_context, const UA_NodeId *method_id, void *method_context,
                  const UA_NodeId *object_id, void *object_context, size_t input_size, const UA_Variant *input, size_t output_size,
                  UA_Variant *output) {
    FileContext *context = (FileContext *)object_context;
    if(!context)
        return UA_STATUSCODE_BADNOTFOUND;
    OpenHandle *handle = findHandle(context, *(const UA_UInt32 *)input[0].data);
    const UA_UInt64 position = *(const UA_UInt64 *)input[1].data;
    if(!handle)
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    if(position > LONG_MAX || fseek(handle->file, (long)position, SEEK_SET) != 0)
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
addFileNode(UA_Server *server, const char *name, const UA_NodeId requested_node_id, UA_NodeId *file_node_id, FileContext **file_context) {
    FileContext *context = (FileContext *)calloc(1, sizeof(FileContext));
    if(!context)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    snprintf(context->path, sizeof(context->path), "%s/%s", data_dir, name);

    UA_ObjectAttributes attr = UA_ObjectAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("en-US", (char *)name);
    const UA_StatusCode status =
        UA_Server_addObjectNode(server, requested_node_id, files_node_id, UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                UA_QUALIFIEDNAME(1, (char *)name), UA_NODEID_NUMERIC(0, UA_NS0ID_FILETYPE), attr, context, file_node_id);
    if(status != UA_STATUSCODE_GOOD) {
        free(context);
        return status;
    }
    context->next = file_contexts;
    file_contexts = context;
    if(file_context)
        *file_context = context;
    UA_LOG_INFO(logger, UA_LOGCATEGORY_USERLAND, "Added file node for %s", context->path);
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
createFileMethod(UA_Server *server, const UA_NodeId *session_id, void *session_context, const UA_NodeId *method_id, void *method_context,
                 const UA_NodeId *object_id, void *object_context, size_t input_size, const UA_Variant *input, size_t output_size,
                 UA_Variant *output) {
    const UA_String *ua_name = (const UA_String *)input[0].data;
    const UA_Boolean request_file_open = *(const UA_Boolean *)input[1].data;
    if(ua_name->length == 0 || ua_name->length >= NAME_MAX || memchr(ua_name->data, '/', ua_name->length))
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    char name[NAME_MAX];
    memcpy(name, ua_name->data, ua_name->length);
    name[ua_name->length] = '\0';
    if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", data_dir, name);
    struct stat file_stat;
    if(stat(path, &file_stat) == 0)
        return UA_STATUSCODE_BADBROWSENAMEDUPLICATED;
    FILE *file = fopen(path, "wb");
    if(!file)
        return UA_STATUSCODE_BADINTERNALERROR;
    fclose(file);

    UA_NodeId file_node_id;
    FileContext *context = NULL;
    UA_StatusCode status = addFileNode(server, name, UA_NODEID_NUMERIC(1, 0), &file_node_id, &context);
    if(status != UA_STATUSCODE_GOOD)
        return status;

    UA_UInt32 handle_id = 0;
    if(request_file_open)
        status = openHandle(context, FILE_MODE_READ | FILE_MODE_WRITE, &handle_id);
    if(status == UA_STATUSCODE_GOOD)
        status = UA_Variant_setScalarCopy(&output[0], &file_node_id, &UA_TYPES[UA_TYPES_NODEID]);
    if(status == UA_STATUSCODE_GOOD)
        status = UA_Variant_setScalarCopy(&output[1], &handle_id, &UA_TYPES[UA_TYPES_UINT32]);
    UA_NodeId_clear(&file_node_id);
    UA_LOG_INFO(logger, UA_LOGCATEGORY_USERLAND, "CreateFile %s: %s", path, UA_StatusCode_name(status));
    return status;
}

static UA_StatusCode
addExistingFiles(UA_Server *server) {
    DIR *dir = opendir(data_dir);
    if(!dir) {
        UA_LOG_ERROR(logger, UA_LOGCATEGORY_USERLAND, "Cannot open directory %s", data_dir);
        return UA_STATUSCODE_BADINTERNALERROR;
    }
    UA_StatusCode status = UA_STATUSCODE_GOOD;
    struct dirent *entry;
    while(status == UA_STATUSCODE_GOOD && (entry = readdir(dir)) != NULL) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", data_dir, entry->d_name);
        struct stat file_stat;
        if(stat(path, &file_stat) != 0 || !S_ISREG(file_stat.st_mode))
            continue;
        status = addFileNode(server, entry->d_name, UA_NODEID_STRING(1, entry->d_name), NULL, NULL);
    }
    closedir(dir);
    return status;
}

static UA_StatusCode
setupAddressSpace(UA_Server *server) {
    UA_ObjectAttributes attr = UA_ObjectAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Files");
    UA_StatusCode status =
        UA_Server_addObjectNode(server, files_node_id, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                UA_QUALIFIEDNAME(1, "Files"), UA_NODEID_NUMERIC(0, UA_NS0ID_FILEDIRECTORYTYPE), attr, NULL, NULL);

    /* Instances reference the methods of their type definition, so the callbacks are set on the methods of the types */
    const struct {
        UA_UInt32 method_id;
        UA_MethodCallback callback;
    } methods[] = {
        {UA_NS0ID_FILETYPE_OPEN, openMethod},
        {UA_NS0ID_FILETYPE_CLOSE, closeMethod},
        {UA_NS0ID_FILETYPE_READ, readMethod},
        {UA_NS0ID_FILETYPE_WRITE, writeMethod},
        {UA_NS0ID_FILETYPE_GETPOSITION, getPositionMethod},
        {UA_NS0ID_FILETYPE_SETPOSITION, setPositionMethod},
        {UA_NS0ID_FILEDIRECTORYTYPE_CREATEFILE, createFileMethod},
    };
    for(size_t i = 0; status == UA_STATUSCODE_GOOD && i < sizeof(methods) / sizeof(methods[0]); ++i)
        status = UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, methods[i].method_id), methods[i].callback);

    if(status == UA_STATUSCODE_GOOD)
        status = addExistingFiles(server);
    return status;
}

int
main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <data directory>\n", argv[0]);
        return EXIT_FAILURE;
    }
    data_dir = argv[1];

    UA_Server *server = UA_Server_new();
    logger = UA_Server_getConfig(server)->logging;

    UA_StatusCode status = setupAddressSpace(server);
    if(status == UA_STATUSCODE_GOOD)
        status = UA_Server_runUntilInterrupt(server);
    else
        UA_LOG_ERROR(logger, UA_LOGCATEGORY_USERLAND, "Failed to set up the address space: %s", UA_StatusCode_name(status));

    UA_Server_delete(server);
    while(file_contexts) {
        FileContext *next = file_contexts->next;
        for(size_t i = 0; i < MAX_OPEN_HANDLES; ++i) {
            if(file_contexts->handles[i].file)
                fclose(file_contexts->handles[i].file);
        }
        free(file_contexts);
        file_contexts = next;
    }
    return status == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
