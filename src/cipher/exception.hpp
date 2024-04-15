#pragma once

#include "common/platform.hpp"

#include <stdexcept>
#include <sstream>
#include <iomanip>

#ifndef NT_SUCCESS
#define SUCCESS(Status) (((STATUS)(Status)) >= 0)
#else
#define SUCCESS(Status) NT_SUCCESS(Status)
#endif

namespace fb2k_ncm::cipher
{
    class cipher_error : public std::runtime_error {
    public:
        explicit cipher_error(std::string reason, STATUS status)
            : std::runtime_error(""), status_(status), reason_(std::move(reason)) {
            last_error_ = GetLastError();
            std::stringstream err_ss;
            err_ss << "cipher runtime error because of [" << reason_ << "] \n"
                   << "  Last Error: 0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex
                   << last_error_ << std::endl
                   << "  STATUS: 0x" << status_;
            msg_ = err_ss.str();
        }
        const char *what() const noexcept override { return msg_.c_str(); }

    public:
        STATUS status_;
        DWORD last_error_;

    protected:
        std::string reason_;
        std::string msg_;
    };
} // namespace fb2k_ncm::cipher