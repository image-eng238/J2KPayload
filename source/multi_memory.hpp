// C++17以上
// 型のサイズとアラインメントのみでポインタを制御(サイズとアラインメントが同じなら異なる型でも1つのポインタで管理)
#pragma once

#include <array>
#include <cassert>
#include <cstdlib>
#include <utility>
#include <cstddef>

namespace MultiMem {
    template <typename First, typename... After>
    class multi_memory {
        struct PDataConst {
            constexpr PDataConst() : size(0), alignment(0) {}
            size_t size;
            size_t alignment;
        };
        struct ReMapData {
            constexpr ReMapData() : size(0), alignment(0), postion(0) {}
            size_t size;
            size_t alignment;
            size_t postion;
        };

    private:
        /*compile start*/
        static constexpr size_t template_parameter_pack_size = sizeof...(After) + 1;

        // コンパイル時にテンプレート引数のサイズとアラインメントを定数化するための構造体
        template <typename Head, typename... Tail>
        struct _MemMake {
            constexpr _MemMake() : data{}, idx{} {
                idx                 = template_parameter_pack_size - (sizeof...(Tail) + 1);
                data[idx].alignment = alignof(Head);
                data[idx].size      = (sizeof(Head) > data[idx].alignment) ? sizeof(Head) : data[idx].alignment;
                if constexpr (sizeof...(Tail) != 0) {
                    constexpr _MemMake<Tail...> nest;
                    for (size_t i = nest.idx; i < template_parameter_pack_size; ++i) {
                        data[i].alignment = nest.data[i].alignment;
                        data[i].size      = nest.data[i].size;
                    }
                }
            }
            std::array<PDataConst, template_parameter_pack_size> data;
            size_t idx;
        };

        // 各テンプレート引数の大きさの重複を除いた数
        static constexpr size_t number_of_data_size = [] {
            _MemMake<First, After...> siz;
            for (size_t step = 0; step < siz.data.size() - 1; ++step) {
                for (size_t idx = 0; idx < siz.data.size() - step - 1; ++idx) {
                    if (siz.data[idx].size > siz.data[idx + 1].size) {
                        PDataConst tmp              = siz.data[idx];
                        siz.data[idx].size          = siz.data[idx + 1].size;
                        siz.data[idx].alignment     = siz.data[idx + 1].alignment;
                        siz.data[idx + 1].size      = tmp.size;
                        siz.data[idx + 1].alignment = tmp.alignment;
                    }
                }
            }
            size_t out = 1;
            for (size_t i = 0; i < template_parameter_pack_size - 1; ++i) {
                if (siz.data[i].size != siz.data[i + 1].size) {
                    ++out;
                }
            }
            return out;
        }();

        // 各テンプレート引数の大きさを昇順で重複を除いた配列
        // 各テンプレート型の size, alignment をコンパイル時に格納
        static constexpr auto ptr_data = [] {
            _MemMake<First, After...> dummy;
            for (size_t step = 0; step < dummy.data.size() - 1; ++step) {
                for (size_t idx = 0; idx < dummy.data.size() - step - 1; ++idx) {
                    if (dummy.data[idx].size > dummy.data[idx + 1].size) {
                        PDataConst tmp                = dummy.data[idx];
                        dummy.data[idx].size          = dummy.data[idx + 1].size;
                        dummy.data[idx].alignment     = dummy.data[idx + 1].alignment;
                        dummy.data[idx + 1].size      = tmp.size;
                        dummy.data[idx + 1].alignment = tmp.alignment;
                    }
                }
            }
            std::array<PDataConst, number_of_data_size> out{};
            if (template_parameter_pack_size != 1) {
                for (size_t i = 0, out_i = 0; i < template_parameter_pack_size - 1; ++i) {
                    if (dummy.data[i].size != dummy.data[i + 1].size) {
                        out[out_i].alignment = dummy.data[i].alignment;
                        out[out_i].size      = dummy.data[i].size;
                        ++out_i;
                    }
                    if (i + 2 == template_parameter_pack_size) {
                        out[out_i].alignment = dummy.data[i + 1].alignment;
                        out[out_i].size      = dummy.data[i + 1].size;
                    }
                }
            } else {
                out[0].alignment = dummy.data[0].alignment;
                out[0].size      = dummy.data[0].size;
            }

            return out;
        }();

        template <typename Head, typename... Tail>
        struct _MemReMap {
            constexpr _MemReMap() : data{}, idx{} {
                idx                 = template_parameter_pack_size - (sizeof...(Tail) + 1);
                data[idx].postion   = idx;
                data[idx].alignment = alignof(Head);
                data[idx].size      = std::max(sizeof(Head), alignof(Head));
                if constexpr (sizeof...(Tail) != 0) {
                    constexpr _MemReMap<Tail...> nest;
                    for (size_t i = nest.idx; i < template_parameter_pack_size; ++i) {
                        data[i].alignment = nest.data[i].alignment;
                        data[i].size      = nest.data[i].size;
                        data[i].postion   = nest.data[i].postion;
                    }
                }
            }
            std::array<ReMapData, template_parameter_pack_size> data;
            size_t idx;
        };

        // コンストラクタ引数からptr_dataの格納先へマッピングする配列
        static constexpr auto raw2remap = [] {
            _MemReMap<First, After...> dummy;
            for (size_t step = 0; step < dummy.data.size() - 1; ++step) {
                for (size_t idx = 0; idx < dummy.data.size() - step - 1; ++idx) {
                    if (dummy.data[idx].size > dummy.data[idx + 1].size) {
                        ReMapData tmp                 = dummy.data[idx];
                        dummy.data[idx].size          = dummy.data[idx + 1].size;
                        dummy.data[idx].alignment     = dummy.data[idx + 1].alignment;
                        dummy.data[idx].postion       = dummy.data[idx + 1].postion;
                        dummy.data[idx + 1].size      = tmp.size;
                        dummy.data[idx + 1].alignment = tmp.alignment;
                        dummy.data[idx + 1].postion   = tmp.postion;
                    }
                }
            }
            return dummy.data;
        }();
        /*compile end*/
    public:
        // multi_memory() = delete;
        multi_memory() : var_ptr{nullptr}, mem_ptr{nullptr} {}
        template <typename... Args>
        multi_memory(const size_t& first_template_size, Args... args)
            : /*var_size{first_template_size, static_cast<size_t>(args)...}, sum_size{},*/ var_ptr{nullptr}, mem_ptr(nullptr) {
            static_assert(template_parameter_pack_size == 1 + sizeof...(args));

            const size_t var_size[template_parameter_pack_size] =
                {first_template_size, static_cast<size_t>(args)...}; // 各テンプレート型の確保するメモリ数
            size_t sum_size = 0;                                     // 確保したメモリの合計サイズ

            // アラインメントを合わせながらsum_sizeを計算
            for (size_t i = 0, pidx = 1; i < template_parameter_pack_size; ++i) {
                assert(var_size[raw2remap[i].postion] > 0);
                sum_size += var_size[raw2remap[i].postion] * raw2remap[i].size;
                if (size_t mod = sum_size % raw2remap[i].alignment; mod != 0) {
                    sum_size += raw2remap[i].alignment - mod;
                }
                if (pidx != number_of_data_size && raw2remap[i].size != raw2remap[i + 1].size) {
                    var_ptr[pidx++] = reinterpret_cast<void*>(sum_size);
                }
            }
            // メモリを取る
            if (mem_ptr = malloc(sum_size); mem_ptr == nullptr) {
                sum_size = 0;
                return;
            }
            // それぞれのポインタをセット
            const ptrdiff_t o_ptr = reinterpret_cast<ptrdiff_t>(mem_ptr);
            for (size_t i = 0; i < number_of_data_size; ++i) {
                const ptrdiff_t c_ptr = reinterpret_cast<ptrdiff_t>(var_ptr[i]);
                var_ptr[i]            = reinterpret_cast<void*>(o_ptr + c_ptr);
            }
        }

        ~multi_memory() { free(mem_ptr); }

        template <typename... Args>
        void allocate(const size_t& first_template_size, Args... args) {
            static_assert(template_parameter_pack_size == 1 + sizeof...(args));

            const size_t var_size[template_parameter_pack_size] =
                {first_template_size, static_cast<size_t>(args)...}; // 各テンプレート型の確保するメモリ数
            size_t sum_size = 0;                                     // 確保したメモリの合計サイズ

            // アラインメントを合わせながらsum_sizeを計算
            for (size_t i = 0, pidx = 1; i < template_parameter_pack_size; ++i) {
                assert(var_size[raw2remap[i].postion] > 0);
                sum_size += var_size[raw2remap[i].postion] * raw2remap[i].size;
                if (size_t mod = sum_size % raw2remap[i].alignment; mod != 0) {
                    sum_size += raw2remap[i].alignment - mod;
                }
                if (pidx != number_of_data_size && raw2remap[i].size != raw2remap[i + 1].size) {
                    var_ptr[pidx++] = reinterpret_cast<void*>(sum_size);
                }
            }
            // メモリを取る
            if (mem_ptr = malloc(sum_size); mem_ptr == nullptr) {
                sum_size = 0;
                return;
            }
            // それぞれのポインタをセット
            const ptrdiff_t o_ptr = reinterpret_cast<ptrdiff_t>(mem_ptr);
            for (size_t i = 0; i < number_of_data_size; ++i) {
                const ptrdiff_t c_ptr = reinterpret_cast<ptrdiff_t>(var_ptr[i]);
                var_ptr[i]            = reinterpret_cast<void*>(o_ptr + c_ptr);
            }
        }

        template <typename T>
        [[nodiscard]] T* get(const size_t& size) {
            static constexpr size_t ptr_index = [] {
                const size_t search = (sizeof(T) > alignof(T)) ? sizeof(T) : alignof(T);
                size_t out          = 0;
                while (ptr_data[out].size != search) ++out;
                return out;
            }();

            T* out             = reinterpret_cast<T*>(var_ptr[ptr_index]);
            ptrdiff_t next_p   = reinterpret_cast<ptrdiff_t>(var_ptr[ptr_index]) + (ptr_data[ptr_index].size * size);
            var_ptr[ptr_index] = reinterpret_cast<void*>(next_p);
            return new (out) T[size]();
        }
        template <typename T, typename... ConstArgs>
        [[nodiscard]] T* get(const size_t& size, ConstArgs&&... args) {
            static constexpr size_t ptr_index = [] {
                const size_t search = (sizeof(T) > alignof(T)) ? sizeof(T) : alignof(T);
                size_t out          = 0;
                while (ptr_data[out].size != search) ++out;
                return out;
            }();

            T* out             = reinterpret_cast<T*>(var_ptr[ptr_index]);
            ptrdiff_t next_p   = reinterpret_cast<ptrdiff_t>(var_ptr[ptr_index]) + (ptr_data[ptr_index].size * size);
            var_ptr[ptr_index] = reinterpret_cast<void*>(next_p);
            for (size_t i = 0; i < size; ++i) {
                new (out + i) T(std::forward<ConstArgs>(args)...);
            }
            return out;
        }
        bool is_empty() const { return mem_ptr == nullptr; }

    private:
        // const size_t var_size[template_parameter_pack_size]; // 各テンプレート型の確保するメモリ数
        // size_t sum_size;                                     // 確保したメモリの合計サイズ
        void* var_ptr[number_of_data_size]; // 各テンプレート型のポインタ インクリメントする
        void* mem_ptr;                      // 確保したメモリの原点のポインタ インクリメントしない
    };

    template <size_t Size>
    class static_memory {
    public:
        static_memory() : pos{0}, data{0} {}

        template <typename T>
        [[nodiscard]] T* get() {
            if (const ptrdiff_t mod = reinterpret_cast<ptrdiff_t>(data + pos) % alignof(T); mod != 0) pos += alignof(T) - mod;
            T* const out         = static_cast<T*>(static_cast<void*>(&data[pos]));
            constexpr size_t siz = (sizeof(T) > alignof(T)) ? sizeof(T) : alignof(T);
            pos += siz;
            assert(pos <= Size);
            return new (out) T[]();
        }
        template <typename T>
        [[nodiscard]] T* get(const size_t& size) {
            if (const ptrdiff_t mod = reinterpret_cast<ptrdiff_t>(data + pos) % alignof(T); mod != 0) pos += alignof(T) - mod;
            T* const out         = static_cast<T*>(static_cast<void*>(&data[pos]));
            constexpr size_t siz = (sizeof(T) > alignof(T)) ? sizeof(T) : alignof(T);
            pos += siz * size;
            assert(pos <= Size);
            return new (out) T[size]();
        }
        template <typename T, typename... ConstArge>
        [[nodiscard]] T* get(const size_t& size, ConstArge&&... args) {
            if (const ptrdiff_t mod = reinterpret_cast<ptrdiff_t>(data + pos) % alignof(T); mod != 0) pos += alignof(T) - mod;
            T* const out         = static_cast<T*>(static_cast<void*>(&data[pos]));
            constexpr size_t siz = (sizeof(T) > alignof(T)) ? sizeof(T) : alignof(T);
            pos += siz * size;
            assert(pos <= Size);
            for (size_t i = 0; i < size; ++i) {
                new (out + i) T(std::forward<ConstArge>(args)...);
            }
            return out;
        }

    private:
        size_t pos;
        uint8_t data[Size];
    };

    template <typename T>
    class multi_ptr {
    public:
        multi_ptr() : ptr{nullptr}, length{0} {}

        template <typename... Args>
        multi_ptr(multi_memory<Args...>& in, const size_t& length) {
            this->length = length;
            ptr          = in.template get<T>(this->length);
        }

        ~multi_ptr() {
            for (size_t i = 0; i < length; ++i) ptr[i].~T();
        }
        template <typename... MemT, typename... Args>
        T* set(multi_memory<MemT...>& in, const size_t& length, Args&&... args) {
            this->length = length;
            return ptr   = in.template get<T>(this->length, std::forward<Args>(args)...);
        }
        template <size_t Size, typename... Args>
        T* set(static_memory<Size>& in, const size_t& length, Args&&... args) {
            this->length = length;
            return ptr   = in.template get<T>(this->length, std::forward<Args>(args)...);
        }
        T* get() const { return ptr; }
        T& operator*() const { return *ptr; }
        T* operator->() const { return ptr; }
        T* operator=(T* in) { return this->ptr = in; }
        T& operator[](const size_t& i) const { return ptr[i]; }
        operator bool() const { return (ptr != nullptr) ? true : false; }

    private:
        T* ptr;
        size_t length;
    };

    template <typename T>
    constexpr bool operator==(const multi_ptr<T>& left, std::nullptr_t) { return !left; }
    template <typename T>
    constexpr bool operator==(std::nullptr_t, const multi_ptr<T>& right) { return right == nullptr; }
    template <typename T>
    constexpr bool operator!=(const multi_ptr<T>& left, std::nullptr_t) { return left; }
    template <typename T>
    constexpr bool operator!=(std::nullptr_t, const multi_ptr<T>& right) { return right != nullptr; }

}