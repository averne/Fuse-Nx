// Copyright (C) 2020 averne
//
// This file is part of fuse-nx.
//
// fuse-nx is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// fuse-nx is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fuse-nx.  If not, see <http://www.gnu.org/licenses/>.

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <utility>

#include <fnx.hpp>

template <typename ...Args>
inline void Py_VarDECREF(Args &&...args) {
    auto decref = [](auto *obj) { Py_DECREF(obj); };
    (decref(args), ...);
}

template <typename ...Args>
inline void Py_VarXDECREF(Args &&...args) {
    auto xdecref = [](auto *obj) { Py_DECREF(obj); };
    (xdecref(args), ...);
}

template <typename ...Args>
inline void Py_VarINCREF(Args &&...args) {
    auto incref = [](auto *obj) { Py_INCREF(obj); };
    (incref(args), ...);
}

template <typename ...Args>
inline void Py_VarXINCREF(Args &&...args) {
    auto xincref = [](auto *obj) { Py_XINCREF(obj); };
    (xincref(args), ...);
}

struct PyFileBase {
    PyObject_HEAD
    std::unique_ptr<fnx::io::FileBase> ptr;
};

static int PyFileBase_init(PyFileBase *self, PyObject *args, PyObject *kwds) {
    char *path = nullptr, *mode = nullptr;
    if (!PyArg_ParseTuple(args, "ss", &path, &mode))
        return 1;

    auto obj = std::make_unique<fnx::io::File>(path, mode);

    if (!obj->good()) {
        PyErr_Format(PyExc_RuntimeError, "%s stream is not good", path);
        return 1;
    }

    obj->update_size();

    self->ptr = std::move(obj);
    return 0;
}

static void PyFileBase_dealloc(PyFileBase *self) {
    self->ptr.reset();
    Py_TYPE(self)->tp_free(self);
}

static PyObject *PyFileBase_size(PyFileBase *self, [[maybe_unused]] PyObject *args) {
    return PyLong_FromUnsignedLong(self->ptr->size());
}

static PyObject *PyFileBase_seek(PyFileBase *self, PyObject **args, Py_ssize_t nargs) {
    if (nargs != 2)
        return nullptr;

    std::size_t where = 0;
    fnx::io::Whence whence = fnx::io::Whence::Set;
    if (!_PyArg_ParseStack(args, nargs, "ii", &where, &whence))
        return nullptr;

    self->ptr->seek(where, whence);
    Py_RETURN_NONE;
}

static PyObject *PyFileBase_tell(PyFileBase *self, [[maybe_unused]] PyObject *args) {
    return PyLong_FromUnsignedLong(self->ptr->tell());
}

static PyObject *PyFileBase_read(PyFileBase *self, PyObject **args, Py_ssize_t nargs) {
    if (nargs != 1)
        return nullptr;

    Py_buffer buffer;
    if (!_PyArg_ParseStack(args, nargs, "y*", &buffer))
        return nullptr;

    FNX_SCOPEGUARD([&buffer] { PyBuffer_Release(&buffer); });
    auto size = std::clamp(static_cast<std::uint64_t>(buffer.len), static_cast<std::uint64_t>(0),
        self->ptr->size() - self->ptr->tell());
    return PyLong_FromUnsignedLong(self->ptr->read(buffer.buf, size));
}

static PyObject *PyFileBase_write(PyFileBase *self, PyObject **args, Py_ssize_t nargs) {
    if (nargs != 1)
        return nullptr;

    Py_buffer buffer;
    if (!_PyArg_ParseStack(args, nargs, "y*", &buffer))
        return nullptr;

    FNX_SCOPEGUARD([&buffer] { PyBuffer_Release(&buffer); });
    return PyLong_FromUnsignedLong(self->ptr->write(buffer.buf, buffer.len));
}

static std::array PyFileBase_methods = {
    PyMethodDef{
        .ml_name  = "size",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyFileBase_size),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Returns size of the file"
    },
    PyMethodDef{
        .ml_name  = "seek",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyFileBase_seek),
        .ml_flags = METH_FASTCALL,
        .ml_doc   = "Sets file position"
    },
    PyMethodDef{
        .ml_name  = "tell",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyFileBase_tell),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Returns file position"
    },
    PyMethodDef{
        .ml_name  = "read",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyFileBase_read),
        .ml_flags = METH_FASTCALL,
        .ml_doc   = "Reads file data"
    },
    PyMethodDef{
        .ml_name  = "write",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyFileBase_write),
        .ml_flags = METH_FASTCALL,
        .ml_doc   = "Write data to file"
    },
    PyMethodDef{ nullptr },
};

static PyTypeObject PyFileBaseType = {
    .ob_base      = PyVarObject_HEAD_INIT(nullptr, 0)
    .tp_name      = "fnxbinds.FileBase",
    .tp_basicsize = sizeof(PyFileBase),
    .tp_dealloc   = reinterpret_cast<destructor>(PyFileBase_dealloc),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "File object",
    .tp_methods   = PyFileBase_methods.data(),
    .tp_init      = reinterpret_cast<initproc>(PyFileBase_init),
    .tp_new       = PyType_GenericNew,
};

struct PyPfsEntry {
    PyObject_HEAD
    PyObject *name;
    std::size_t offset, size;
};

static void PyPfsEntry_dealloc(PyPfsEntry *self) {
    Py_VarXDECREF(self->name);
    Py_TYPE(self)->tp_free(self);
}

static std::array PyPfsEntry_members = {
    PyMemberDef{
        .name   = "name",
        .type   = T_OBJECT_EX,
        .offset = offsetof(PyPfsEntry, name),
        .doc    = "Entry name",
    },
    PyMemberDef{
        .name   = "offset",
        .type   = T_ULONG,
        .offset = offsetof(PyPfsEntry, offset),
        .doc    = "Entry offset",
    },
    PyMemberDef{
        .name   = "size",
        .type   = T_ULONG,
        .offset = offsetof(PyPfsEntry, size),
        .doc    = "Entry size",
    },
    PyMemberDef{ nullptr },
};

static PyTypeObject PyPfsEntryType = {
    .ob_base      = PyVarObject_HEAD_INIT(nullptr, 0)
    .tp_name      = "fnxbinds.PfsEntry",
    .tp_basicsize = sizeof(PyPfsEntry),
    .tp_dealloc   = reinterpret_cast<destructor>(PyPfsEntry_dealloc),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Pfs entry object",
    .tp_members   = PyPfsEntry_members.data(),
    .tp_new       = PyType_GenericNew,
};

struct PyPfs {
    PyObject_HEAD
    std::unique_ptr<fnx::hac::Pfs> ptr;
};

static int PyPfs_init(PyPfs *self, PyObject *args, PyObject *kwds) {
    PyFileBase *base = nullptr;
    if (!PyArg_ParseTuple(args, "O", &base))
        return 1;

    self->ptr = std::make_unique<fnx::hac::Pfs>(base->ptr->clone());
    return 0;
}

static void PyPfs_dealloc(PyPfs *self) {
    self->ptr.reset();
    Py_TYPE(self)->tp_free(self);
}

static PyObject *PyPfs_is_valid(PyPfs *self, [[maybe_unused]] PyObject *args) {
    if (self->ptr->is_valid())
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyPfs_parse(PyPfs *self, [[maybe_unused]] PyObject *args) {
    if (self->ptr->parse())
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyPfs_get_entries(PyPfs *self, [[maybe_unused]] PyObject *args) {
    auto *dict = PyDict_New();
    if (!dict)
        return nullptr;

    for (auto &entry: self->ptr->get_entries()) {
        auto *obj = PyObject_New(PyPfsEntry, &PyPfsEntryType);
        if (!obj)
            continue;

        obj->name   = nullptr;
        obj->offset = entry.offset;
        obj->size   = entry.size;

        obj->name = PyUnicode_FromStringAndSize(entry.name.data(), entry.name.size());
        if (!obj->name) {
            Py_VarDECREF(obj);
            continue;
        }

        PyDict_SetItem(dict, obj->name, reinterpret_cast<PyObject *>(obj));
        Py_VarDECREF(obj);
    }

    return dict;
}

static PyObject *PyPfs_open(PyPfs *self, PyObject **args, Py_ssize_t nargs) {
    if (nargs != 1)
        return nullptr;

    PyPfsEntry *entry = nullptr;
    if (!_PyArg_ParseStack(args, nargs, "O", &entry))
        return nullptr;

    auto *file = PyObject_New(PyFileBase, &PyFileBaseType);
    if (!file)
        return nullptr;

    std::memset(reinterpret_cast<void *>(&file->ptr), 0, sizeof(file->ptr));
    file->ptr = self->ptr->open({ entry->offset, entry->size });
    return reinterpret_cast<PyObject *>(file);
}

static std::array PyPfs_methods = {
    PyMethodDef{
        .ml_name  = "is_valid",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyPfs_is_valid),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Checks file magic"
    },
    PyMethodDef{
        .ml_name  = "parse",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyPfs_parse),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Parses the Pfs"
    },
    PyMethodDef{
        .ml_name  = "get_entries",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyPfs_get_entries),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Gets dict of entries"
    },
    PyMethodDef{
        .ml_name  = "open",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyPfs_open),
        .ml_flags = METH_FASTCALL,
        .ml_doc   = "Opens entry"
    },
    PyMethodDef{ nullptr },
};

static PyTypeObject PyPfsType = {
    .ob_base      = PyVarObject_HEAD_INIT(nullptr, 0)
    .tp_name      = "fnxbinds.Pfs",
    .tp_basicsize = sizeof(PyPfs),
    .tp_dealloc   = reinterpret_cast<destructor>(PyPfs_dealloc),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Pfs object",
    .tp_methods   = PyPfs_methods.data(),
    .tp_init      = reinterpret_cast<initproc>(PyPfs_init),
    .tp_new       = PyType_GenericNew,
};

struct PyHfsEntry {
    PyObject_HEAD
    PyObject *name;
    std::size_t offset, size;
};

static void PyHfsEntry_dealloc(PyHfsEntry *self) {
    Py_VarXDECREF(self->name);
    Py_TYPE(self)->tp_free(self);
}

static std::array PyHfsEntry_members = {
    PyMemberDef{
        .name   = "name",
        .type   = T_OBJECT_EX,
        .offset = offsetof(PyHfsEntry, name),
        .doc    = "Entry name",
    },
    PyMemberDef{
        .name   = "offset",
        .type   = T_ULONG,
        .offset = offsetof(PyHfsEntry, offset),
        .doc    = "Entry offset",
    },
    PyMemberDef{
        .name   = "size",
        .type   = T_ULONG,
        .offset = offsetof(PyHfsEntry, size),
        .doc    = "Entry size",
    },
    PyMemberDef{ nullptr },
};

static PyTypeObject PyHfsEntryType = {
    .ob_base      = PyVarObject_HEAD_INIT(nullptr, 0)
    .tp_name      = "fnxbinds.HfsEntry",
    .tp_basicsize = sizeof(PyHfsEntry),
    .tp_dealloc   = reinterpret_cast<destructor>(PyHfsEntry_dealloc),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Hfs entry object",
    .tp_members   = PyHfsEntry_members.data(),
    .tp_new       = PyType_GenericNew,
};

struct PyHfs {
    PyObject_HEAD
    std::unique_ptr<fnx::hac::Hfs> ptr;
};

static int PyHfs_init(PyHfs *self, PyObject *args, PyObject *kwds) {
    PyFileBase *base = nullptr;
    if (!PyArg_ParseTuple(args, "O", &base))
        return 1;

    self->ptr = std::make_unique<fnx::hac::Hfs>(base->ptr->clone());
    return 0;
}

static void PyHfs_dealloc(PyHfs *self) {
    self->ptr.reset();
    Py_TYPE(self)->tp_free(self);
}

static PyObject *PyHfs_is_valid(PyHfs *self, [[maybe_unused]] PyObject *args) {
    if (self->ptr->is_valid())
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyHfs_parse(PyHfs *self, [[maybe_unused]] PyObject *args) {
    if (self->ptr->parse())
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyHfs_get_entries(PyHfs *self, [[maybe_unused]] PyObject *args) {
    auto *dict = PyDict_New();
    if (!dict)
        return nullptr;

    for (auto &entry: self->ptr->get_entries()) {
        auto *obj = PyObject_New(PyHfsEntry, &PyHfsEntryType);
        if (!obj)
            continue;

        obj->name   = nullptr;
        obj->offset = entry.offset;
        obj->size   = entry.size;

        obj->name = PyUnicode_FromStringAndSize(entry.name.data(), entry.name.size());
        if (!obj->name) {
            Py_VarDECREF(obj);
            continue;
        }

        PyDict_SetItem(dict, obj->name, reinterpret_cast<PyObject *>(obj));
        Py_VarDECREF(obj);
    }

    return dict;
}

static PyObject *PyHfs_open(PyHfs *self, PyObject **args, Py_ssize_t nargs) {
    if (nargs != 1)
        return nullptr;

    PyHfsEntry *entry = nullptr;
    if (!_PyArg_ParseStack(args, nargs, "O", &entry))
        return nullptr;

    auto *file = PyObject_New(PyFileBase, &PyFileBaseType);
    if (!file)
        return nullptr;

    std::memset(reinterpret_cast<void *>(&file->ptr), 0, sizeof(file->ptr));
    file->ptr = self->ptr->open({ entry->offset, entry->size });
    return reinterpret_cast<PyObject *>(file);
}

static std::array PyHfs_methods = {
    PyMethodDef{
        .ml_name  = "is_valid",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyHfs_is_valid),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Checks file magic"
    },
    PyMethodDef{
        .ml_name  = "parse",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyHfs_parse),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Parses the Hfs"
    },
    PyMethodDef{
        .ml_name  = "get_entries",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyHfs_get_entries),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Gets dict of entries"
    },
    PyMethodDef{
        .ml_name  = "open",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyHfs_open),
        .ml_flags = METH_FASTCALL,
        .ml_doc   = "Opens entry"
    },
    PyMethodDef{ nullptr },
};

static PyTypeObject PyHfsType = {
    .ob_base      = PyVarObject_HEAD_INIT(nullptr, 0)
    .tp_name      = "fnxbinds.Hfs",
    .tp_basicsize = sizeof(PyHfs),
    .tp_dealloc   = reinterpret_cast<destructor>(PyHfs_dealloc),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Hfs object",
    .tp_methods   = PyHfs_methods.data(),
    .tp_init      = reinterpret_cast<initproc>(PyHfs_init),
    .tp_new       = PyType_GenericNew,
};

struct PyRomfsFileEntry {
    PyObject_HEAD
    PyObject *name;
    PyObject *parent;
    std::size_t offset, size;
};

static void PyRomfsFileEntry_dealloc(PyRomfsFileEntry *self) {
    PyObject_GC_UnTrack(self);
    Py_VarXDECREF(self->name, self->parent);
    PyObject_GC_Del(self);
}

static int PyRomfsFileEntry_traverse(PyRomfsFileEntry *self, visitproc visit, void *arg) {
    Py_VISIT(self->parent);
    return 0;
}

static int PyRomfsFileEntry_clear(PyRomfsFileEntry *self) {
    Py_CLEAR(self->parent);
    return 0;
}

static std::array PyRomfsFileEntry_members = {
    PyMemberDef{
        .name   = "name",
        .type   = T_OBJECT_EX,
        .offset = offsetof(PyRomfsFileEntry, name),
        .doc    = "Entry name",
    },
    PyMemberDef{
        .name   = "parent",
        .type   = T_OBJECT_EX,
        .offset = offsetof(PyRomfsFileEntry, parent),
        .doc    = "Entry parent",
    },
    PyMemberDef{
        .name   = "offset",
        .type   = T_ULONG,
        .offset = offsetof(PyRomfsFileEntry, offset),
        .doc    = "Entry offset",
    },
    PyMemberDef{
        .name   = "size",
        .type   = T_ULONG,
        .offset = offsetof(PyRomfsFileEntry, size),
        .doc    = "Entry size",
    },
    PyMemberDef{ nullptr },
};

static PyTypeObject PyRomfsFileEntryType = {
    .ob_base      = PyVarObject_HEAD_INIT(nullptr, 0)
    .tp_name      = "fnxbinds.RomfsFileEntry",
    .tp_basicsize = sizeof(PyRomfsFileEntry),
    .tp_dealloc   = reinterpret_cast<destructor>(PyRomfsFileEntry_dealloc),
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc       = "RomFs file entry object",
    .tp_traverse  = reinterpret_cast<traverseproc>(PyRomfsFileEntry_traverse),
    .tp_clear     = reinterpret_cast<inquiry>(PyRomfsFileEntry_clear),
    .tp_members   = PyRomfsFileEntry_members.data(),
};

struct PyRomfsDirEntry {
    PyObject_HEAD
    PyObject *name;
    PyObject *parent;
    PyObject *children;
    PyObject *files;
};

static void PyRomfsDirEntry_dealloc(PyRomfsDirEntry *self) {
    PyObject_GC_UnTrack(self);
    Py_VarXDECREF(self->name, self->parent, self->children, self->files);
    PyObject_GC_Del(self);
}

static int PyRomfsDirEntry_traverse(PyRomfsDirEntry *self, visitproc visit, void *arg) {
    Py_VISIT(self->parent);
    Py_VISIT(self->children);
    Py_VISIT(self->files);
    return 0;
}

static int PyRomfsDirEntry_clear(PyRomfsDirEntry *self) {
    Py_CLEAR(self->parent);
    Py_CLEAR(self->children);
    Py_CLEAR(self->files);
    return 0;
}

static std::array PyRomfsDirEntry_members = {
    PyMemberDef{
        .name   = "name",
        .type   = T_OBJECT_EX,
        .offset = offsetof(PyRomfsDirEntry, name),
        .doc    = "Entry name",
    },
    PyMemberDef{
        .name   = "parent",
        .type   = T_OBJECT_EX,
        .offset = offsetof(PyRomfsDirEntry, parent),
        .doc    = "Entry parent",
    },
    PyMemberDef{
        .name   = "children",
        .type   = T_OBJECT_EX,
        .offset = offsetof(PyRomfsDirEntry, children),
        .doc    = "Entry children",
    },
    PyMemberDef{
        .name   = "files",
        .type   = T_OBJECT_EX,
        .offset = offsetof(PyRomfsDirEntry, files),
        .doc    = "Entry files",
    },
    PyMemberDef{ nullptr },
};

static PyTypeObject PyRomfsDirEntryType = {
    .ob_base      = PyVarObject_HEAD_INIT(nullptr, 0)
    .tp_name      = "fnxbinds.RomfsDirEntry",
    .tp_basicsize = sizeof(PyRomfsDirEntry),
    .tp_dealloc   = reinterpret_cast<destructor>(PyRomfsDirEntry_dealloc),
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc       = "RomFs directory entry object",
    .tp_traverse  = reinterpret_cast<traverseproc>(PyRomfsDirEntry_traverse),
    .tp_clear     = reinterpret_cast<inquiry>(PyRomfsDirEntry_clear),
    .tp_members   = PyRomfsDirEntry_members.data(),
};

struct PyRomfs {
    PyObject_HEAD
    std::unique_ptr<fnx::hac::RomFs> ptr;
};

static int PyRomfs_init(PyRomfs *self, PyObject *args, PyObject *kwds) {
    PyFileBase *base = nullptr;
    if (!PyArg_ParseTuple(args, "O", &base))
        return 1;

    self->ptr = std::make_unique<fnx::hac::RomFs>(base->ptr->clone());
    return 0;
}

static void PyRomfs_dealloc(PyRomfs *self) {
    self->ptr.reset();
    Py_TYPE(self)->tp_free(self);
}

static PyObject *PyRomfs_is_valid(PyRomfs *self, [[maybe_unused]] PyObject *args) {
    if (self->ptr->is_valid())
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyRomfs_parse(PyRomfs *self, [[maybe_unused]] PyObject *args) {
    if (self->ptr->parse_full())
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyRomfs_get_entries(PyRomfs *self, [[maybe_unused]] PyObject *args) {
    auto *file_dict = PyDict_New();
    if (!file_dict)
        return nullptr;

    auto *dir_dict = PyDict_New();
    if (!dir_dict)
        return Py_VarDECREF(file_dict), nullptr;

    std::function<bool(fnx::hac::RomFs::DirEntry *dir, PyRomfsDirEntry *parent)> walk =
    [file_dict, dir_dict, &walk](fnx::hac::RomFs::DirEntry *dir, PyRomfsDirEntry *parent) -> bool {
        for (auto &entry: dir->files) {
            auto *obj = PyObject_GC_New(PyRomfsFileEntry, &PyRomfsFileEntryType);
            if (!entry)
                return false;

            obj->name   = nullptr;
            obj->parent = reinterpret_cast<PyObject *>(parent); Py_VarINCREF(parent);
            obj->offset = entry->offset;
            obj->size   = entry->size;

            obj->name = PyUnicode_FromStringAndSize(entry->name.data(), entry->name.size());
            if (!obj->name)
                return Py_VarDECREF(obj), false;

            PyObject_GC_Track(obj);

            auto path_str = entry->path();
            auto *path = PyUnicode_FromStringAndSize(path_str.c_str(), path_str.size());
            if (!path)
                return Py_VarDECREF(obj), false;

            if (PyList_Append(parent->files, reinterpret_cast<PyObject *>(obj)) < 0)
                return false;

            if (PyDict_SetItem(file_dict, path, reinterpret_cast<PyObject *>(obj)) < 0)
                return Py_VarDECREF(path, obj), false;
            Py_VarDECREF(path, obj);
        }

        for (auto &entry: dir->children) {
            auto *obj = PyObject_GC_New(PyRomfsDirEntry, &PyRomfsDirEntryType);
            if (!obj)
                return false;

            obj->name   = obj->children = obj->files = nullptr;
            obj->parent = reinterpret_cast<PyObject *>(parent); Py_VarINCREF(parent);

            obj->name = PyUnicode_FromStringAndSize(entry->name.data(), entry->name.size());
            if (!obj->name)
                return Py_VarDECREF(obj), false;

            obj->children = PyList_New(0);
            if (!obj->children)
                return Py_VarDECREF(obj), false;

            obj->files = PyList_New(0);
            if (!obj->files)
                return Py_VarDECREF(obj), false;

            PyObject_GC_Track(obj);

            auto path_str = entry->path();
            auto *path = PyUnicode_FromStringAndSize(path_str.c_str(), path_str.size());
            if (!path)
                return Py_VarDECREF(obj), false;

            if (PyList_Append(parent->children, reinterpret_cast<PyObject *>(obj)) < 0)
                return Py_VarDECREF(obj), false;

            if (PyDict_SetItem(dir_dict, path, reinterpret_cast<PyObject *>(obj)) < 0)
                return Py_VarDECREF(path, obj), false;
            Py_VarDECREF(path, obj);

            if (!walk(entry, obj))
                return false;
        }

        return true;
    };

    // Build and insert root entry before processing the rest of the filesystem
    auto *root_obj = PyObject_GC_New(PyRomfsDirEntry, &PyRomfsDirEntryType);
    if (!root_obj)
        return Py_VarDECREF(file_dict, dir_dict), nullptr;

    root_obj->name = root_obj->children = root_obj->files = nullptr;
    root_obj->parent = Py_None; Py_VarINCREF(Py_None);

    root_obj->name = PyUnicode_FromStringAndSize("", std::strlen(""));
    if (!root_obj->name)
        return Py_VarDECREF(root_obj, file_dict, dir_dict), nullptr;

    root_obj->children = PyList_New(0);
    if (!root_obj->children)
        return Py_VarDECREF(root_obj, file_dict, dir_dict), nullptr;

    root_obj->files = PyList_New(0);
    if (!root_obj->files)
        return Py_VarDECREF(root_obj, file_dict, dir_dict), nullptr;

    PyObject_GC_Track(root_obj);

    auto *path = PyUnicode_FromStringAndSize("/", std::strlen("/"));
    if (!path)
        return Py_VarDECREF(root_obj, file_dict, dir_dict), nullptr;

    PyDict_SetItem(dir_dict, path, reinterpret_cast<PyObject *>(root_obj));
    Py_VarDECREF(path, root_obj);

    auto &root_entry = self->ptr->get_root();
    walk(root_entry.get(), root_obj);
    FNX_SCOPEGUARD([&] { Py_VarDECREF(file_dict, dir_dict); });
    return Py_BuildValue("(OO)", file_dict, dir_dict);
}

static PyObject *PyRomfs_open(PyRomfs *self, PyObject **args, Py_ssize_t nargs) {
    if (nargs != 1)
        return nullptr;

    PyRomfsFileEntry *entry = nullptr;
    if (!_PyArg_ParseStack(args, nargs, "O", &entry))
        return nullptr;

    auto *file = PyObject_New(PyFileBase, &PyFileBaseType);
    if (!file)
        return nullptr;

    fnx::hac::RomFs::FileEntry meta;
    meta.offset = entry->offset;
    meta.size   = entry->size;

    std::memset(reinterpret_cast<void *>(&file->ptr), 0, sizeof(file->ptr));
    file->ptr = self->ptr->open(meta);
    return reinterpret_cast<PyObject *>(file);
}

static std::array PyRomfs_methods = {
    PyMethodDef{
        .ml_name  = "is_valid",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyRomfs_is_valid),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Checks file magic"
    },
    PyMethodDef{
        .ml_name  = "parse",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyRomfs_parse),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Parses the RomFs"
    },
    PyMethodDef{
        .ml_name  = "get_entries",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyRomfs_get_entries),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Gets dicts of file and dir entries"
    },
    PyMethodDef{
        .ml_name  = "open",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyRomfs_open),
        .ml_flags = METH_FASTCALL,
        .ml_doc   = "Opens entry"
    },
    PyMethodDef{ nullptr },
};

static PyTypeObject PyRomfsType = {
    .ob_base      = PyVarObject_HEAD_INIT(nullptr, 0)
    .tp_name      = "fnxbinds.Romfs",
    .tp_basicsize = sizeof(PyRomfs),
    .tp_dealloc   = reinterpret_cast<destructor>(PyRomfs_dealloc),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Romfs object",
    .tp_methods   = PyRomfs_methods.data(),
    .tp_init      = reinterpret_cast<initproc>(PyRomfs_init),
    .tp_new       = PyType_GenericNew,
};

struct PyNca {
    PyObject_HEAD
    std::unique_ptr<fnx::hac::Nca> ptr;
};

static int PyNca_init(PyNca *self, PyObject *args, PyObject *kwds) {
    PyFileBase *base = nullptr;
    if (!PyArg_ParseTuple(args, "O", &base))
        return 1;

    self->ptr = std::make_unique<fnx::hac::Nca>(base->ptr->clone());
    return 0;
}

static void PyNca_dealloc(PyNca *self) {
    self->ptr.reset();
    Py_TYPE(self)->tp_free(self);
}

static PyObject *PyNca_is_valid(PyNca *self, [[maybe_unused]] PyObject *args) {
    if (self->ptr->is_valid())
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyNca_parse(PyNca *self, [[maybe_unused]] PyObject *args) {
    if (self->ptr->parse())
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyNca_get_distribution_type(PyNca *self, [[maybe_unused]] PyObject *args) {
    return PyLong_FromUnsignedLong(static_cast<unsigned long>(self->ptr->get_distribution_type()));
}

static PyObject *PyNca_get_content_type(PyNca *self, [[maybe_unused]] PyObject *args) {
    return PyLong_FromUnsignedLong(static_cast<unsigned long>(self->ptr->get_content_type()));
}

static PyObject *PyNca_get_size(PyNca *self, [[maybe_unused]] PyObject *args) {
    return PyLong_FromUnsignedLong(self->ptr->get_size());
}

static PyObject *PyNca_get_title_id(PyNca *self, [[maybe_unused]] PyObject *args) {
    return PyLong_FromUnsignedLong(self->ptr->get_title_id());
}

static PyObject *PyNca_get_sdk_ver(PyNca *self, [[maybe_unused]] PyObject *args) {
    auto &&[major, minor, micro, rev] = self->ptr->get_sdk_ver();
    return Py_BuildValue("(iiii)", major, minor, micro, rev);
}

static PyObject *PyNca_get_rights_id(PyNca *self, [[maybe_unused]] PyObject *args) {
    return Py_BuildValue("y#", reinterpret_cast<const char *>(self->ptr->get_rights_id().data()), 0x10);
}

static PyObject *PyNca_get_sections(PyNca *self, [[maybe_unused]] PyObject *args) {
    auto *list = PyList_New(0);
    if (!list)
        return nullptr;

    for (auto &section: self->ptr->get_sections()) {
        if (section.get_type() == fnx::hac::Nca::Section::Type::Pfs) {
            auto *pfs = PyObject_New(PyPfs, &PyPfsType);
            if (!pfs)
                continue;

            std::memset(reinterpret_cast<void *>(&pfs->ptr), 0, sizeof(pfs->ptr));
            pfs->ptr = std::make_unique<fnx::hac::Pfs>(section.get_pfs().clone_base());

            PyList_Append(list, reinterpret_cast<PyObject *>(pfs));
            Py_VarDECREF(pfs);
        } else {
            auto *romfs = PyObject_New(PyRomfs, &PyRomfsType);
            if (!romfs)
                continue;

            std::memset(reinterpret_cast<void *>(&romfs->ptr), 0, sizeof(romfs->ptr));
            romfs->ptr = std::make_unique<fnx::hac::RomFs>(section.get_romfs().clone_base());

            PyList_Append(list, reinterpret_cast<PyObject *>(romfs));
            Py_VarDECREF(romfs);
        }
    }

    return list;
}

static std::array PyNca_methods = {
    PyMethodDef{
        .ml_name  = "is_valid",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyNca_is_valid),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Checks file magic"
    },
    PyMethodDef{
        .ml_name  = "parse",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyNca_parse),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Parses the Nca"
    },
    PyMethodDef{
        .ml_name  = "get_distribution_type",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyNca_get_distribution_type),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Gets the distribution type of the Nca"
    },
    PyMethodDef{
        .ml_name  = "get_content_type",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyNca_get_content_type),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Gets the content type of the Nca"
    },
    PyMethodDef{
        .ml_name  = "get_size",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyNca_get_size),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Gets the size of the Nca as described in its header"
    },
    PyMethodDef{
        .ml_name  = "get_title_id",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyNca_get_title_id),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Gets the titleid associated with the Nca"
    },
    PyMethodDef{
        .ml_name  = "get_sdk_ver",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyNca_get_sdk_ver),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Gets the SDK version of the Nca"
    },
    PyMethodDef{
        .ml_name  = "get_rights_id",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyNca_get_rights_id),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Gets the rightsid associated with the Nca"
    },
    PyMethodDef{
        .ml_name  = "get_sections",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyNca_get_sections),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Gets list of sections"
    },
    PyMethodDef{ nullptr },
};

static PyTypeObject PyNcaType = {
    .ob_base      = PyVarObject_HEAD_INIT(nullptr, 0)
    .tp_name      = "fnxbinds.Nca",
    .tp_basicsize = sizeof(PyNca),
    .tp_dealloc   = reinterpret_cast<destructor>(PyNca_dealloc),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Nca object",
    .tp_methods   = PyNca_methods.data(),
    .tp_init      = reinterpret_cast<initproc>(PyNca_init),
    .tp_new       = PyType_GenericNew,
};

struct PyXci {
    PyObject_HEAD
    std::unique_ptr<fnx::hac::Xci> ptr;
};

static int PyXci_init(PyXci *self, PyObject *args, PyObject *kwds) {
    PyFileBase *base = nullptr;
    if (!PyArg_ParseTuple(args, "O", &base))
        return 1;

    self->ptr = std::make_unique<fnx::hac::Xci>(base->ptr->clone());
    return 0;
}

static void PyXci_dealloc(PyXci *self) {
    self->ptr.reset();
    Py_TYPE(self)->tp_free(self);
}

static PyObject *PyXci_is_valid(PyXci *self, [[maybe_unused]] PyObject *args) {
    if (self->ptr->is_valid())
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyXci_parse(PyXci *self, [[maybe_unused]] PyObject *args) {
    if (self->ptr->parse())
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *PyXci_get_cart_type(PyXci *self, [[maybe_unused]] PyObject *args) {
    return PyLong_FromUnsignedLong(static_cast<unsigned long>(self->ptr->get_cart_type()));
}

static PyObject *PyXci_get_partitions(PyXci *self, [[maybe_unused]] PyObject *args) {
    auto *dict = PyDict_New();
    if (!dict)
        return nullptr;

    for (auto &part: self->ptr->get_partitions()) {
        auto *obj = PyObject_New(PyHfs, &PyHfsType);
        if (!obj)
            continue;

        auto name_str = part.get_name();
        auto *name = PyUnicode_FromStringAndSize(name_str.data(), name_str.size());
        if (!name) {
            Py_VarDECREF(obj);
            continue;
        }

        std::memset(reinterpret_cast<void *>(&obj->ptr), 0, sizeof(obj->ptr));
        obj->ptr = std::make_unique<fnx::hac::Hfs>(part.get_hfs().clone_base());

        PyDict_SetItem(dict, name, reinterpret_cast<PyObject *>(obj));
        Py_VarDECREF(name, obj);
    }

    return dict;
}

static std::array PyXci_methods = {
    PyMethodDef{
        .ml_name  = "is_valid",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyXci_is_valid),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Checks file magic"
    },
    PyMethodDef{
        .ml_name  = "parse",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyXci_parse),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Parses the Xci"
    },
    PyMethodDef{
        .ml_name  = "get_cart_type",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyXci_get_cart_type),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Gets the cartridge type of the Xci"
    },
    PyMethodDef{
        .ml_name  = "get_partitions",
        .ml_meth  = reinterpret_cast<PyCFunction>(PyXci_get_partitions),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Gets dict of partitions"
    },
    PyMethodDef{ nullptr },
};

static PyTypeObject PyXciType = {
    .ob_base      = PyVarObject_HEAD_INIT(nullptr, 0)
    .tp_name      = "fnxbinds.Xci",
    .tp_basicsize = sizeof(PyXci),
    .tp_dealloc   = reinterpret_cast<destructor>(PyXci_dealloc),
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Xci object",
    .tp_methods   = PyXci_methods.data(),
    .tp_init      = reinterpret_cast<initproc>(PyXci_init),
    .tp_new       = PyType_GenericNew,
};

static PyObject *fnxbinds_set_key(PyObject *self, PyObject **args, Py_ssize_t nargs) {
    if (nargs != 2)
        return nullptr;

    char *name_str = nullptr, *key_str = nullptr;
    Py_ssize_t name_size = 0, key_size = 0;
    if (!_PyArg_ParseStack(args, nargs, "s#s#", &name_str, &name_size, &key_str, &key_size))
        return nullptr;

    fnx::crypt::KeySet::get()->set_key({ name_str, static_cast<std::size_t>(name_size) },
        { key_str, static_cast<std::size_t>(key_size) });
    Py_RETURN_NONE;
}

static PyObject *fnxbinds_set_titlekey(PyObject *self, PyObject **args, Py_ssize_t nargs) {
    if (nargs != 2)
        return nullptr;

    char *name_str = nullptr, *key_str = nullptr;
    Py_ssize_t name_size = 0, key_size = 0;
    if (!_PyArg_ParseStack(args, nargs, "s#s#", &name_str, &name_size, &key_str, &key_size))
        return nullptr;

    fnx::crypt::TitlekeySet::get()->set_key({ name_str, static_cast<std::size_t>(name_size) },
        { key_str, static_cast<std::size_t>(key_size) });
    Py_RETURN_NONE;
}

static PyObject *fnxbinds_set_user_titlekey(PyObject *self, PyObject **args, Py_ssize_t nargs) {
    if (nargs != 1)
        return nullptr;

    char *key_str = nullptr;
    Py_ssize_t key_size = 0;
    if (!_PyArg_ParseStack(args, nargs, "s#", &key_str, &key_size))
        return nullptr;

    fnx::crypt::TitlekeySet::get()->set_cli_key({ key_str, static_cast<std::size_t>(key_size) });
    Py_RETURN_NONE;
}

static PyObject *fnxbinds_remove_user_titlekey(PyObject *self, PyObject *args) {
    fnx::crypt::TitlekeySet::get()->remove_cli_key();
    Py_RETURN_NONE;
}

static PyObject *fnxbinds_match(PyObject *self, PyObject **args, Py_ssize_t nargs) {
    PyFileBase *base = nullptr;
    if (!_PyArg_ParseStack(args, nargs, "O", &base))
        return nullptr;

    return PyLong_FromLong(static_cast<long>(fnx::hac::match(base->ptr->read(0x400))));
}

static std::array fnxbinds_methods = {
    PyMethodDef{
        .ml_name  = "set_key",
        .ml_meth  = reinterpret_cast<PyCFunction>(fnxbinds_set_key),
        .ml_flags = METH_FASTCALL,
        .ml_doc   = "Sets a prod/dev key"
    },
    PyMethodDef{
        .ml_name  = "set_titlekey",
        .ml_meth  = reinterpret_cast<PyCFunction>(fnxbinds_set_titlekey),
        .ml_flags = METH_FASTCALL,
        .ml_doc   = "Sets a titlekey for a rightsid"
    },
    PyMethodDef{
        .ml_name  = "set_user_titlekey",
        .ml_meth  = reinterpret_cast<PyCFunction>(fnxbinds_set_user_titlekey),
        .ml_flags = METH_FASTCALL,
        .ml_doc   = "Sets the titlekey that will be used for every rightsid"
    },
    PyMethodDef{
        .ml_name  = "remove_user_titlekey",
        .ml_meth  = reinterpret_cast<PyCFunction>(fnxbinds_remove_user_titlekey),
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Removes the titlekey that will be used for every rightsid"
    },
    PyMethodDef{
        .ml_name  = "match",
        .ml_meth  = reinterpret_cast<PyCFunction>(fnxbinds_match),
        .ml_flags = METH_FASTCALL,
        .ml_doc   = "Returns an id based on the format of the file"
    },
    PyMethodDef{ nullptr },
};

static PyModuleDef fnxmodule = {
    .m_base    = PyModuleDef_HEAD_INIT,
    .m_name    = "fnxbinds",
    .m_doc     = "Fnx bindings",
    .m_size    = -1,
    .m_methods = fnxbinds_methods.data(),
};

PyMODINIT_FUNC PyInit_fnxbinds() {
    fnx::crypt::KeySet::set(std::make_unique<fnx::crypt::KeySet>());
    fnx::crypt::TitlekeySet::set(std::make_unique<fnx::crypt::TitlekeySet>());

    PyObject *m = nullptr;

    if (PyType_Ready(&PyFileBaseType) < 0)
        goto exit;

    if (PyType_Ready(&PyPfsEntryType) < 0)
        goto exit;

    if (PyType_Ready(&PyPfsType) < 0)
        goto exit;

    if (PyType_Ready(&PyHfsEntryType) < 0)
        goto exit;

    if (PyType_Ready(&PyHfsType) < 0)
        goto exit;

    if (PyType_Ready(&PyRomfsFileEntryType) < 0)
        goto exit;

    if (PyType_Ready(&PyRomfsDirEntryType) < 0)
        goto exit;

    if (PyType_Ready(&PyRomfsType) < 0)
        goto exit;

    if (PyType_Ready(&PyNcaType) < 0)
        goto exit;

    if (PyType_Ready(&PyXciType) < 0)
        goto exit;

    m = PyModule_Create(&fnxmodule);
    if (!m)
        goto exit;

    Py_VarINCREF(&PyFileBaseType);
    PyModule_AddObject(m, "FileBase", reinterpret_cast<PyObject *>(&PyFileBaseType));

    Py_VarINCREF(&PyPfsEntryType);
    PyModule_AddObject(m, "PfsEntry", reinterpret_cast<PyObject *>(&PyPfsEntryType));

    Py_VarINCREF(&PyPfsType);
    PyModule_AddObject(m, "Pfs", reinterpret_cast<PyObject *>(&PyPfsType));

    Py_VarINCREF(&PyHfsEntryType);
    PyModule_AddObject(m, "HfsEntry", reinterpret_cast<PyObject *>(&PyHfsEntryType));

    Py_VarINCREF(&PyHfsType);
    PyModule_AddObject(m, "Hfs", reinterpret_cast<PyObject *>(&PyHfsType));

    Py_VarINCREF(&PyRomfsFileEntryType);
    PyModule_AddObject(m, "RomfsFileEntry", reinterpret_cast<PyObject *>(&PyRomfsFileEntryType));

    Py_VarINCREF(&PyRomfsDirEntryType);
    PyModule_AddObject(m, "RomfsDirEntry", reinterpret_cast<PyObject *>(&PyRomfsDirEntryType));

    Py_VarINCREF(&PyRomfsType);
    PyModule_AddObject(m, "Romfs", reinterpret_cast<PyObject *>(&PyRomfsType));

    Py_VarINCREF(&PyNcaType);
    PyModule_AddObject(m, "Nca", reinterpret_cast<PyObject *>(&PyNcaType));

    Py_VarINCREF(&PyXciType);
    PyModule_AddObject(m, "Xci", reinterpret_cast<PyObject *>(&PyXciType));

    return m;

exit:
    Py_VarXDECREF(m);
    return nullptr;
}
