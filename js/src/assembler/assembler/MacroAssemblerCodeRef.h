/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef MacroAssemblerCodeRef_h
#define MacroAssemblerCodeRef_h

#include "assembler/wtf/Platform.h"
#include "assembler/jit/ExecutableAllocator.h"

#if ENABLE_ASSEMBLER

// ASSERT_VALID_CODE_POINTER checks that ptr is a non-null pointer, and that it is a valid
// instruction address on the platform (for example, check any alignment requirements).
#if WTF_CPU_ARM_THUMB2
// ARM/thumb instructions must be 16-bit aligned, but all code pointers to be loaded
// into the processor are decorated with the bottom bit set, indicating that this is
// thumb code (as oposed to 32-bit traditional ARM).  The first test checks for both
// decorated and undectorated null, and the second test ensures that the pointer is
// decorated.
#define ASSERT_VALID_CODE_POINTER(ptr) \
    ASSERT(reinterpret_cast<intptr_t>(ptr) & ~1); \
    ASSERT(reinterpret_cast<intptr_t>(ptr) & 1)
#define ASSERT_VALID_CODE_OFFSET(offset) \
    ASSERT(!(offset & 1)) // Must be multiple of 2.
#else
#define ASSERT_VALID_CODE_POINTER(ptr) \
    ASSERT(ptr)
#define ASSERT_VALID_CODE_OFFSET(offset) // Anything goes!
#endif

namespace JSC {

// FunctionPtr:
//
// FunctionPtr should be used to wrap pointers to C/C++ functions in JSC
// (particularly, the stub functions).
class FunctionPtr {
public:
    FunctionPtr()
        : m_value(0)
    {
    }

    template<typename FunctionType>
    explicit FunctionPtr(FunctionType* value)
#if WTF_COMPILER_RVCT
     // RVTC compiler needs C-style cast as it fails with the following error
     // Error:  #694: reinterpret_cast cannot cast away const or other type qualifiers
        : m_value((void*)(value))
#else
        : m_value(reinterpret_cast<void*>(value))
#endif
    {
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    void* value() const { return m_value; }
    void* executableAddress() const { return m_value; }


private:
    void* m_value;
};

// ReturnAddressPtr:
//
// ReturnAddressPtr should be used to wrap return addresses generated by processor
// 'call' instructions exectued in JIT code.  We use return addresses to look up
// exception and optimization information, and to repatch the call instruction
// that is the source of the return address.
class ReturnAddressPtr {
public:
    ReturnAddressPtr()
        : m_value(0)
    {
    }

    explicit ReturnAddressPtr(void* value)
        : m_value(value)
    {
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    explicit ReturnAddressPtr(FunctionPtr function)
        : m_value(function.value())
    {
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    void* value() const { return m_value; }

private:
    void* m_value;
};

// MacroAssemblerCodePtr:
//
// MacroAssemblerCodePtr should be used to wrap pointers to JIT generated code.
class MacroAssemblerCodePtr {
public:
    MacroAssemblerCodePtr()
        : m_value(0)
    {
    }

    explicit MacroAssemblerCodePtr(void* value)
#if WTF_CPU_ARM_THUMB2
        // Decorate the pointer as a thumb code pointer.
        : m_value(reinterpret_cast<char*>(value) + 1)
#else
        : m_value(value)
#endif
    {
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    explicit MacroAssemblerCodePtr(ReturnAddressPtr ra)
        : m_value(ra.value())
    {
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    void* executableAddress() const { return m_value; }
#if WTF_CPU_ARM_THUMB2
    // To use this pointer as a data address remove the decoration.
    void* dataLocation() const { ASSERT_VALID_CODE_POINTER(m_value); return reinterpret_cast<char*>(m_value) - 1; }
#else
    void* dataLocation() const { ASSERT_VALID_CODE_POINTER(m_value); return m_value; }
#endif

    bool operator!()
    {
        return !m_value;
    }

private:
    void* m_value;
};

// MacroAssemblerCodeRef:
//
// A reference to a section of JIT generated code.  A CodeRef consists of a
// pointer to the code, and a ref pointer to the pool from within which it
// was allocated.
class MacroAssemblerCodeRef {
public:
    MacroAssemblerCodeRef()
        : m_size(0)
    {
    }

    MacroAssemblerCodeRef(void* code, ExecutablePool* executablePool, size_t size)
        : m_code(code)
        , m_executablePool(executablePool)
        , m_size(size)
    {
    }

    MacroAssemblerCodePtr m_code;
    ExecutablePool* m_executablePool;
    size_t m_size;
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif // MacroAssemblerCodeRef_h
