#pragma once

#include <winerror.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace fb2k_ncm::cipher
{
    class cipher_error : public std::runtime_error {
    public:
        explicit cipher_error(std::string reason, NTSTATUS status)
            : std::runtime_error(""), status_(status), reason_(std::move(reason)) {
            last_error_ = GetLastError();
            std::stringstream err_ss;
            err_ss << "cipher runtime error because of [" << reason_ << "] \n"
                   << "  Last Error: 0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex
                   << last_error_ << std::endl
                   << "  NTSTATUS: 0x" << status_;
            msg_ = err_ss.str();
        }
        const char *what() const override { return msg_.c_str(); }

    public:
        NTSTATUS status_;
        DWORD last_error_;

    protected:
        std::string reason_;
        std::string msg_;
    };
} // namespace fb2k_ncm::cipher