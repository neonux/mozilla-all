/*  GRAPHITE2 LICENSING

    Copyright 2010, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, 51 Franklin Street, 
    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the 
    internet at http://www.fsf.org/licenses/lgpl.html.

Alternatively, the contents of this file may be used under the terms of the
Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
License, as published by the Free Software Foundation, either version 2
of the License or (at your option) any later version.
*/
// This class represents loaded graphite stack machine code.  It performs 
// basic sanity checks, on the incoming code to prevent more obvious problems
// from crashing graphite.
// Author: Tim Eves

#pragma once

#include <cassert>
#include <graphite2/Types.h>
#include "Main.h"
#include "Machine.h"

namespace graphite2 {

class Silf;
class Face;

namespace vm {

class Machine::Code
{
public:
    enum status_t 
    {
        loaded,
        alloc_failed, 
        invalid_opcode, 
        unimplemented_opcode_used,
        out_of_range_data,
        jump_past_end,
        arguments_exhausted,
        missing_return
    };

private:
    class decoder;

    instr *     _code;
    byte  *     _data;
    size_t      _data_size,
                _instr_count;
    byte        _max_ref;
    mutable status_t _status;
    bool        _constraint,
                _modify,
                _delete;
    mutable bool _own;

    void release_buffers() throw ();
    void failure(const status_t) throw();

public:
    Code() throw();
    Code(bool is_constraint, const byte * bytecode_begin, const byte * const bytecode_end,
         uint8 pre_context, uint16 rule_length, const Silf &, const Face &);
    Code(const Machine::Code &) throw();
    ~Code() throw();
    
    Code & operator=(const Code &rhs) throw();
    operator bool () const throw();
    status_t      status() const throw();
    bool          constraint() const throw();
    size_t        dataSize() const throw();
    size_t        instructionCount() const throw();
    bool          immutable() const throw();
    bool          deletes() const throw();
    size_t        maxRef() const throw();

    int32 run(Machine &m, slotref * & map) const;
    
    CLASS_NEW_DELETE;
};

inline Machine::Code::Code() throw()
: _code(0), _data(0), _data_size(0), _instr_count(0), _max_ref(0),
  _status(loaded), _own(false) {
}

inline Machine::Code::Code(const Machine::Code &obj) throw ()
 :  _code(obj._code), 
    _data(obj._data), 
    _data_size(obj._data_size), 
    _instr_count(obj._instr_count),
    _max_ref(obj._max_ref),
    _status(obj._status), 
    _constraint(obj._constraint),
    _modify(obj._modify),
    _delete(obj._delete),
    _own(obj._own) 
{
    obj._own = false;
}

inline Machine::Code & Machine::Code::operator=(const Machine::Code &rhs) throw() {
    if (_instr_count > 0)
        release_buffers();
    _code        = rhs._code; 
    _data        = rhs._data;
    _data_size   = rhs._data_size; 
    _instr_count = rhs._instr_count;
    _status      = rhs._status; 
    _constraint  = rhs._constraint;
    _modify      = rhs._modify;
    _delete      = rhs._delete;
    _own         = rhs._own; 
    rhs._own = false;
    return *this;
}

inline Machine::Code::operator bool () const throw () {
    return _code && status() == loaded;
}

inline Machine::Code::status_t Machine::Code::status() const throw() {
    return _status;
}

inline bool Machine::Code::constraint() const throw() {
    return _constraint;
}

inline size_t Machine::Code::dataSize() const throw() {
    return _data_size;
}

inline size_t Machine::Code::instructionCount() const throw() {
    return _instr_count;
}

inline bool Machine::Code::immutable() const throw()
{
  return !(_delete || _modify);
}

inline bool Machine::Code::deletes() const throw()
{
  return _delete;
}

inline size_t Machine::Code::maxRef() const throw()
{
	return _max_ref;
}

} // namespace vm
} // namespace graphite2
