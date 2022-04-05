struct Writer
{
};

struct StorageA
{
};

struct StorageB
{
};

struct StorageC
{
};

template<typename T>
Writer& operator<<(Writer& writer, const T& s)
{
    std::ignore = s;
    return writer;
}


int main()
{
    Writer writer;
    StorageA sA;
    StorageA sB;
    StorageA sC;

    auto _ = [&writer](auto& self, const auto& storage)
    {
        writer << storage;
        return [&](auto& storage) { return self(self, storage); };
    };

    auto __ = [&_](auto& a) { return _(_, a); };

    __(sA)(sB)(sC);
}
