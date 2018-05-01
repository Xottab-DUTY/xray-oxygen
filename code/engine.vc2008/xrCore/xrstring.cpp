#include "stdafx.h"
#pragma hdrstop

#include "xrstring.h"

XRCORE_API	extern		str_container*	g_pStringContainer = nullptr;

struct str_container_impl
{
	static const u32 buffer_size = 1024 * 256;
	str_value*		 buffer[buffer_size];
	int              num_docs;

	str_container_impl()
	{
		num_docs = 0;
		std::memset(buffer, 0, sizeof(buffer));
	}

	str_value*       find(str_value* value, const char* str)
	{
		str_value* candidate = buffer[value->dwCRC % buffer_size];
		while (candidate)
		{
			if (candidate->dwCRC == value->dwCRC &&
				candidate->dwLength == value->dwLength &&
				!memcmp(candidate->value, str, value->dwLength))
			{
				return candidate;
			}

			candidate = candidate->next;
		}

		return nullptr;
	}

	void			 insert(str_value* value)
	{
		str_value** element = &buffer[value->dwCRC % buffer_size];
		value->next = *element;
		*element = value;
	}

	void			 clean()
	{
		for (u32 i = 0; i<buffer_size; ++i)
		{
			str_value** current = &buffer[i];

			while (*current != nullptr)
			{
				str_value* value = *current;
				if (!value->dwReference)
				{
					*current = value->next;
					xr_free(value);
				}
				else
					current = &value->next;
			}
		}
	}

	void			 verify()
	{
		Msg("strings verify started");
		for (u32 i = 0; i<buffer_size; ++i)
		{
			str_value* value = buffer[i];
			while (value)
			{
				u32			crc = crc32(value->value, value->dwLength);
				string32	crc_str;
				R_ASSERT3(crc == value->dwCRC, "CorePanic: read-only memory corruption (shared_strings)", itoa(value->dwCRC, crc_str, 16));
				R_ASSERT3(value->dwLength == xr_strlen(value->value), "CorePanic: read-only memory corruption (shared_strings, internal structures)", value->value);
				value = value->next;
			}
		}
		Msg("strings verify completed");
	}

	void			dump(FILE* f) const
	{
		for (u32 i = 0; i<buffer_size; ++i)
		{
			str_value* value = buffer[i];
			while (value)
			{
				fprintf(f, "ref[%4u]-len[%3u]-crc[%8X] : %s\n", value->dwReference, value->dwLength, value->dwCRC, value->value);
				value = value->next;
			}
		}
	}

	void			dump(IWriter* f) const
	{
		for (u32 i = 0; i<buffer_size; ++i)
		{
			str_value* value = buffer[i];
			string4096		temp;
			while (value)
			{
				xr_sprintf(temp, sizeof(temp), "ref[%4d]-len[%3d]-crc[%8X] : %s\n", value->dwReference, value->dwLength, value->dwCRC, value->value);
				f->w_string(temp);
				value = value->next;
			}
		}
	}

	int				stat_economy()
	{
		int				counter = 0;
		for (u32 i = 0; i<buffer_size; ++i)
		{
			str_value* value = buffer[i];
			while (value)
			{
				counter -= sizeof(str_value);
				counter += (value->dwReference - 1)*(value->dwLength + 1);
				value = value->next;
			}
		}

		return counter;
	}
};

str_container::str_container()
{
	impl = new str_container_impl();
}

str_value*	str_container::dock(str_c value)
{
	if (!value) return 0;

	std::lock_guard<decltype(cs)> lock(cs);

	str_value*	result = 0;

	// calc len
	u32		s_len = xr_strlen(value);
	u32		s_len_with_zero = (u32)s_len + 1;
	VERIFY(sizeof(str_value) + s_len_with_zero < 4096);

	// setup find structure
	char	header[sizeof(str_value)];
	str_value*	sv = (str_value*)header;
	sv->dwReference = 0;
	sv->dwLength = s_len;
	sv->dwCRC = crc32(value, s_len);

	// search
	result = impl->find(sv, value);

#ifdef DEBUG
	bool is_leaked_string = !xr_strcmp(value, "enter leaked string here");
#endif //DEBUG

	// it may be the case, string is not found or has "non-exact" match
	if (0 == result
#ifdef DEBUG
		|| is_leaked_string
#endif //DEBUG
		) {

		result = (str_value*)Memory.mem_alloc(sizeof(str_value) + s_len_with_zero);

#ifdef DEBUG
		static int num_leaked_string = 0;
		if (is_leaked_string)
		{
			++num_leaked_string;
			Msg("leaked_string: %d 0x%08x", num_leaked_string, result);
		}
#endif // DEBUG

		result->dwReference = 0;
		result->dwLength = sv->dwLength;
		result->dwCRC = sv->dwCRC;
		std::memcpy(result->value, value, s_len_with_zero);

		impl->insert(result);
	}

	return	result;
}

void		str_container::clean()
{
	std::lock_guard<decltype(cs)> lock(cs);
	impl->clean();
}

void		str_container::verify()
{
	std::lock_guard<decltype(cs)> lock(cs);
	impl->verify();
}

void		str_container::dump()
{
	std::lock_guard<decltype(cs)> lock(cs);
	FILE* F = fopen("d:\\$str_dump$.txt", "w");
	impl->dump(F);
	fclose(F);
}

void		str_container::dump(IWriter* W)
{
	std::lock_guard<decltype(cs)> lock(cs);
	impl->dump(W);
}

u32			str_container::stat_economy()
{
	std::lock_guard<decltype(cs)> lock(cs);
	int				counter = 0;
	counter -= sizeof(*this);
	counter += impl->stat_economy();
	return			u32(counter);
}

str_container::~str_container()
{
	clean();
	//dump ();
	xr_delete(impl);
}


//xr_string class

xr_vector<xr_string> xr_string::Split(LPCSTR Str, int StrSize, char splitCh)
{
    xr_vector<xr_string> Result;

    int SubStrBeginCursor = 0;
    for (int StrCursor = 0; StrCursor < StrSize; ++StrCursor)
    {
        if (Str[StrCursor] == splitCh)
        {
            //Don't create empty string
            if ((StrCursor - 1 - SubStrBeginCursor) > 0)
            {
                Result.push_back(xr_string(&Str[SubStrBeginCursor], StrCursor - 1));
            }
        }
    }

    return Result;
}

xr_string::xr_string()
    : Super()
{
}

xr_string::xr_string(LPCSTR Str, int Size)
    : Super(Str, Size)
{
}

xr_string::xr_string(const xr_string& other)
    : Super(other)
{
}

xr_string::xr_string(const xr_string&& other)
    : Super(other)
{
}

xr_string::xr_string(const Super&& other)
    : Super(other)
{
}

xr_string::xr_string(LPCSTR Str)
{
    assign(Str);
}

xr_string& xr_string::operator=(LPCSTR Str)
{
    assign(Str);
    return *this;
}

xr_string& xr_string::operator=(const xr_string& other)
{
    assign(other);
    return *this;
}

xr_string& xr_string::operator=(const Super& other)
{
    assign(other);
    return *this;
}

xr_vector<xr_string> xr_string::Split(char splitCh)
{
    return Split(data(), size(), splitCh);
}


xr_string xr_string::RemoveWhitespaces() const
{
    int Size = size();
    if (Size == 0) return xr_string();

    xr_string Result;
    Result.reserve(Size);

    const char* OrigStr = data();

    for (int i = 0; i < Size; ++i)
    {
        if (*OrigStr != ' ')
        {
            Result.push_back(OrigStr[i]);
        }
    }

    return Result;
}

bool xr_string::StartWith(const xr_string& Other) const
{
    return StartWith(Other.data(), Other.size());
}


bool xr_string::StartWith(LPCSTR Str) const
{
    u32 StrLen = xr_strlen(Str);
    return StartWith(Str, (int)StrLen);
}

bool xr_string::StartWith(LPCSTR Str, int Size) const
{
    int OurSize = size();

    //String is greater then our, we can't success
    if (OurSize < Size) return false;

    const char* OurStr = data();

    for (int i = 0; i < Size; ++i)
    {
        if (OurStr[i] != Str[i])
        {
            return false;
        }
    }

    return true;
}
