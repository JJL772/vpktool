
#pragma once

#include <cstdint>
#include <stdexcept>

namespace vpklib {
	namespace util {

		struct ReadContext
		{
			std::uint64_t pos;
			std::uint64_t size;
			const char* data;

			ReadContext(const char* _data, std::uint64_t _size) :
				data(_data),
				size(_size)
			{

			}

			template<class T>
			T read() {
				T* dat = static_cast<T*>(data + pos);
				pos += sizeof(T);
				return dat;
			}

			void read_string(char* buffer) {
				while(data[pos] != 0) {
					if(pos >= size)
						break;
					*buffer = data[pos++];
				}
			}

			void set_pos(std::uint64_t pos) {
				if(pos >= size)
					throw std::out_of_range("set_pos called with value >= size");
				pos = pos;
			}

			auto get_pos() {
				return pos;
			}

			void seek(std::uint64_t forward) {
				if(pos + forward >= size)
					throw std::out_of_range("seek called with value that causes overflow");
				pos += forward;
			}

			void seekr(std::uint64_t back) {
				if (pos - back < 0)
					throw std::out_of_range("seekr called with data that causes a underflow");
				pos -= back;
			}

		};

	}
}
