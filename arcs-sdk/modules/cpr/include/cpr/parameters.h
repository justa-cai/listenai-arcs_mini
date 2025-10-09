#ifndef CPR_PARAMETERS_H
#define CPR_PARAMETERS_H

#include <initializer_list>

#include "cpr/curl_container.h"

namespace cpr {

class Parameters : public CurlContainer<Parameter> {
  public:
    template <class It>
    Parameters(const It begin, const It end) {
        for (It parameter = begin; parameter != end; ++parameter) {
            Add(*parameter);
        }
    }
    Parameters() = default;
    Parameters(const std::initializer_list<Parameter>& parameters) : CurlContainer<Parameter>(parameters) {}
};

} // namespace cpr

#endif
