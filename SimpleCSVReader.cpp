
//usage: auto&& [id, name, gender, price, discount] = tr.readRow<int32_t, std::string, std::string, int32_t, CsvReader::Percent>();

class CsvReader : public TextReader
{
public:
    using TextReader::TextReader;

    template <typename... fields>
    std::tuple<fields...> readRow()
    {
        std::tuple<fields...> ret;
        readTupleField<0, fields...>(ret);
        endOfLine(false);
        return ret;
    }

    struct Percent
    {
        float value;
        Percent& operator = (float v) { value = v; return *this; }
        operator float()
        {
            return value;
        }
    };

private:
    template <std::size_t idx, typename... fields>
    typename std::enable_if<idx >= std::tuple_size<std::tuple<fields...>>::value - 1>::type readTupleField(std::tuple<fields...>& out)
    {
        *this >> std::get<idx>(out);
    }

    template <std::size_t idx, typename... fields>
    typename std::enable_if < idx < std::tuple_size<std::tuple<fields...>>::value - 1>::type readTupleField(std::tuple<fields...>& out)
    {
        *this >> std::get<idx>(out);
        readCharCheck(';');
        readTupleField<idx + 1, fields...>(out);
    }

    CsvReader& operator>>(std::string& result)
    {
        result = this->getCurrentLine();
        result = result.substr(0, result.find(";"));
        this->advancePointer(this->getCurrentLine() + result.size());
        return *this;
    }

    CsvReader& operator>>(int32_t& result)
    {
        result = readInt32();
        return *this;
    }

    CsvReader& operator>>(int64_t& result)
    {
        result = readInt64();
        return *this;
    }

    CsvReader& operator>>(float& result)
    {
        result = readFloat();
        if (peekChar(false) == ',')
        {
            readChar(false);
            auto power = 0.1f;
            for (auto c = peekChar(false); c >= '0' && c <= '9'; power *= 0.1f, c = peekChar(false))
            {
                result += (readChar(false) - '0') * power;
            }
        }
        return *this;
    }

    CsvReader& operator>>(Percent& result)
    {
        *this >> result.value;
        if (!readCharCheck('%'))
        {
            result = 0.f;
        }
        return *this;
    }
};
