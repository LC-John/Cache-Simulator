#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <assert.h>
#include "cache.h"

static unsigned long long getbit(unsigned long long inst, int s,int e)
{
    return (unsigned long long)((unsigned long long)((unsigned long long)inst << (63 - e)) >> (63 - e + s));
}

void Cache::SetConfig(CacheConfig cc)
{
    unsigned int tmp;
    config_ = cc;

    set_num = cc.set_num;
    line_num = cc.associativity;
    line_size = cc.size / set_num / line_num;
    for (tmp = set_num-1, idx_bits = 0; tmp > 0; tmp >>= 1, idx_bits++);
    for (tmp = line_size-1, offset_bits = 0; tmp > 0; tmp >>= 1, offset_bits++);
    tag_bits = 64 - idx_bits - offset_bits;

    valid = new bool*[set_num];
    dirty = new bool*[set_num];
    tag = new unsigned long long*[set_num];
    bytes = new char**[set_num];
    counter = new int*[set_num];
    bt = new int*[set_num];
    mct = new unsigned long long*[set_num];
    for (int i = 0; i < set_num; i++)
    {
        valid[i] = new bool[line_num];
        dirty[i] = new bool[line_num];
        tag[i] = new unsigned long long[line_num];
        bytes[i] = new char*[line_num];
        counter[i] = new int[line_num];
        bt[i] = new int[line_num];
        for (int j = 0; j < line_num; j++)
        {
            valid[i][j] = false;
            dirty[i][j] = false;
            tag[i][j] = 0x0;
            bytes[i][j] = new char[line_size];
            memset(bytes[i][j], 0, line_size*sizeof(char));
            counter[i][j] = 0;
            bt[i][j] = 0;
        }
        mct[i] = new unsigned long long[config_.mct_size];
        for (int j = 0; j < config_.mct_size; j++)
            mct[i][j] = (unsigned long long)-1;
    }
#ifdef DEBUG
    printf("%s + %s\n",
           config_.write_through==0?"WriteBack":"WriteThrough",
           config_.write_allocate==0?"WriteNonAlloc":"WriteAlloc");
    printf("set num = %d    ", set_num);
    printf("line num = %d    ", line_num);
    printf("line size = %d\n", line_size);
    printf("tag bits = %d    ", tag_bits);
    printf("idx bits = %d    ", idx_bits);
    printf("offset bits = %d\n", offset_bits);
    printf("tag[%d:%d] | idx[%d:%d] | offset[%d:%d]\n",
           tag_bits + idx_bits + offset_bits - 1, idx_bits + offset_bits,
           idx_bits + offset_bits - 1, offset_bits,
           offset_bits - 1, 0);
    print();
#endif
}

void Cache::print()
{
    for (int i = 0; i < set_num; i++)
    {
        printf("SET %d:\n", i);
        for (int j = 0; j < line_num; j++)
        {
            printf("  LINE %d: valid = %5s    dirty = %5s    LRUcounter = %d    tag = %llx\n    content:", j,
                   valid[i][j]?"True":"False", dirty[i][j]?"True":"False", counter[i][j], tag[i][j]);
            for (int k = 0; k < line_size; k++)
                printf(" %02x", (unsigned char)bytes[i][j][k]);
            printf("\n");
        }
    }

    printf("access = %d\n", stats_.access_counter);
    printf("miss = %d\n", stats_.miss_num);
    printf("fetch = %d\n", stats_.fetch_num);
    printf("replace = %d\n", stats_.replace_num);
}

unsigned long long Cache::get_tag(unsigned long long addr)
{
    return getbit(addr, idx_bits + offset_bits, tag_bits + idx_bits + offset_bits - 1);
}

unsigned long long Cache::get_idx(unsigned long long addr)
{
    return getbit(addr, offset_bits, idx_bits + offset_bits - 1);
}

unsigned long long Cache::get_offset(unsigned long long addr)
{
    return getbit(addr, 0, offset_bits - 1);
}

int Cache::find_line(unsigned long long arg_tag, int set, int &line)
{
    for (int i = 0; i < line_num; i++)
        if (valid[set][i] && tag[set][i] == arg_tag)
        {
            line = i;
            return 1;
        }
    line = -1;
    return 0;
}

void Cache::read_hit(unsigned long long addr, int arg_set, int arg_line, int arg_offset, int arg_bytes, char *content, int &time)
{
#ifdef DEBUG
    printf("  Cache location: set %d line %d\n", arg_set, arg_line);
#endif
    addr = addr;
    for (int i = 0; i < arg_bytes; i++)
        content[i] = bytes[arg_set][arg_line][arg_offset+i];

    if (config_.replace_policy == REPLACE_LRU)
        LRU_update(arg_set, arg_line);
    else if (config_.replace_policy == REPLACE_PLRU)
        PLRU_update(arg_set, arg_line);
}

void Cache::read_miss(unsigned long long addr, int arg_set, int arg_offset, int arg_bytes, char *content, int &time)
{
    int sub_line = -1, max_cnt = 0;
    bool found = false;

    for (int i = 0; i < line_num; i++)
        if (!valid[arg_set][i]) // empty line found
        {
            found = true;
            sub_line = i;
            break;
        }
        else if (config_.replace_policy == REPLACE_LRU
                 && max_cnt < counter[arg_set][i]) // LRU line found
        {
            max_cnt = counter[arg_set][i];
            sub_line = i;
        }
    if (config_.replace_policy == REPLACE_RANDOM && !found)
        sub_line = rand() % line_num;
    else if (config_.replace_policy == REPLACE_PLRU && !found)
        sub_line = PLRU_sel(arg_set);
#ifdef DEBUG
    if (found)
        printf("  Find empty: set %d line %d\n", arg_set, sub_line);
    else
        printf("  Replace: set %d line %d\n", arg_set, sub_line);
#endif
    if (!found) // replace
    {
        stats_.replace_num++;
        if (config_.use_bypass == 1)
            MCT_update(arg_set, get_tag(addr));
        if (config_.write_through == 0 && dirty[arg_set][sub_line])   // write-back
        {
#ifdef DEBUG
            printf("    Write set %d line %d back\n", arg_set, sub_line);
            printf("      Line addr = %llx, line size = %x\n",
                   ((tag[arg_set][sub_line] << idx_bits) | arg_set) << offset_bits,
                   1 << offset_bits);
#endif
            int wb_hit = 0, wb_time = 0;
            lower_->HandleRequest(((tag[arg_set][sub_line] << idx_bits) | arg_set) << offset_bits,
                                  1 << offset_bits, 0, bytes[arg_set][sub_line], wb_hit, wb_time);
            time += wb_time;
            stats_.miss_time += wb_time;
            stats_.lower_access_counter++;
        }
    }
#ifdef DEBUG
    printf("    Fetch to set %d line %d\n", arg_set, sub_line);
    printf("      Line addr = %llx, line size = %x\n",
           (addr >> offset_bits) << offset_bits,
           1 << offset_bits);
#endif
    valid[arg_set][sub_line] = true;
    dirty[arg_set][sub_line] = false;
    tag[arg_set][sub_line] = get_tag(addr);
    stats_.fetch_num++;

    int lower_hit = 0, lower_time = 0;
    lower_->HandleRequest((addr >> offset_bits) << offset_bits,
                          1 << offset_bits, 1, bytes[arg_set][sub_line], lower_hit, lower_time);
    time += lower_time;
    stats_.miss_time += lower_time;
    stats_.lower_access_counter++;
    read_hit(addr, arg_set, sub_line, arg_offset, arg_bytes, content, time);

    if (config_.use_prefetch == 1)
        prefetch(addr);
}

void Cache::write_hit(unsigned long long addr, int arg_set, int arg_line, int arg_offset, int arg_bytes, char *content, int &time)
{
#ifdef DEBUG
    printf("  Cache location: set %d line %d\n", arg_set, arg_line);
#endif
    if (config_.write_through == 0) // write back
    {
#ifdef DEBUG
        printf("  Make set %d line %d dirty\n", arg_set, arg_line);
#endif
        dirty[arg_set][arg_line] = true;    // dirty
        for (int i = 0; i < arg_bytes; i++)
            bytes[arg_set][arg_line][arg_offset+i] = content[i];

        if (config_.replace_policy == REPLACE_LRU)
            LRU_update(arg_set, arg_line);
        else if (config_.replace_policy == REPLACE_PLRU)
            PLRU_update(arg_set, arg_line);
    }
    else    // write through
    {
        for (int i = 0; i < arg_bytes; i++)
            bytes[arg_set][arg_line][arg_offset+i] = content[i];
#ifdef DEBUG
        printf("  Write set %d line %d through\n", arg_set, arg_line);
        printf("      Line addr = %llx, line size = %x\n",
               ((tag[arg_set][arg_line] << idx_bits) | arg_set) << offset_bits,
               1 << offset_bits);
#endif
        int wt_hit = 0, wt_time = 0;
        lower_->HandleRequest(addr, arg_bytes, 0, content, wt_hit, wt_time);
        time += wt_time;

        if (config_.replace_policy == REPLACE_LRU)
            LRU_update(arg_set, arg_line);
        else if (config_.replace_policy == REPLACE_PLRU)
            PLRU_update(arg_set, arg_line);
    }
}

void Cache::write_miss(unsigned long long addr, int arg_set, int arg_offset, int arg_bytes, char *content, int &time)
{
    int sub_line = -1, max_cnt = 0;
    bool found = false;
    int lower_hit = 0, lower_time = 0;

    if (config_.write_allocate == 1)    // write allocate
    {
        for (int i = 0; i < line_num; i++)
            if (!valid[arg_set][i]) // empty line found
            {
                found = true;
                sub_line = i;
                break;
            }
            else if (config_.replace_policy == REPLACE_LRU && max_cnt < counter[arg_set][i]) // LRU line found
            {
                max_cnt = counter[arg_set][i];
                sub_line = i;
            }
        if (config_.replace_policy == REPLACE_RANDOM && !found)
            sub_line = rand() % line_num;
        else if (config_.replace_policy == REPLACE_PLRU && !found)
            sub_line = PLRU_sel(arg_set);
#ifdef DEBUG
        if (found)
            printf("  Find empty: set %d line %d\n", arg_set, sub_line);
        else
            printf("  Replace: set %d line %d\n", arg_set, sub_line);
#endif
        if (!found) // LRU
        {
            stats_.replace_num++;
            if (config_.use_bypass == 1)
                MCT_update(arg_set, get_tag(addr));
            if (config_.write_through == 0 && dirty[arg_set][sub_line])   // write-back
            {
#ifdef DEBUG
                printf("    Write set %d line %d back\n", arg_set, sub_line);
                printf("      Line addr = %llx, line size = %x\n",
                       ((tag[arg_set][sub_line] << idx_bits) | arg_set) << offset_bits,
                       1 << offset_bits);
#endif
                int wb_hit = 0, wb_time = 0;
                lower_->HandleRequest(((tag[arg_set][sub_line] << idx_bits) | arg_set) << offset_bits,
                                      1 << offset_bits, 0, bytes[arg_set][sub_line], wb_hit, wb_time);
                time += wb_time;
                stats_.lower_access_counter++;
                stats_.miss_time += wb_time;
            }
        }
#ifdef DEBUG
        printf("    Fetch to set %d line %d\n", arg_set, sub_line);
        printf("      Line addr = %llx, line size = %x\n",
               (addr >> offset_bits) << offset_bits,
               1 << offset_bits);
#endif
        valid[arg_set][sub_line] = true;
        dirty[arg_set][sub_line] = false;
        tag[arg_set][sub_line] = get_tag(addr);
        stats_.fetch_num++;
        lower_->HandleRequest((addr >> offset_bits) << offset_bits,
                              1 << offset_bits, 1, bytes[arg_set][sub_line], lower_hit, lower_time);
        time += lower_time;
        stats_.miss_time += lower_time;
        stats_.lower_access_counter++;
        write_hit(addr, arg_set, sub_line, arg_offset, arg_bytes, content, time);

        if (config_.use_prefetch == 1)
            prefetch(addr);
    }
    else    // write non-allocate
    {
#ifdef DEBUG
        printf("  Write to lower\n");
#endif
        lower_->HandleRequest(addr, arg_bytes, 0, content, lower_hit, lower_time);
        time += lower_time;
        stats_.miss_time += lower_time;
        stats_.lower_access_counter++;
    }
}

void Cache::HandleRequest(unsigned long long addr, int arg_bytes,
                          int read, char *content,
                          int &hit, int &time)
{
    unsigned long long _tag = get_tag(addr);
    unsigned long long _idx = get_idx(addr);
    unsigned long long _offset = get_offset(addr);
    int _line;
#ifdef DEBUG
    if (read == 1)
        printf("Read %d byte(s) from %llu\n", arg_bytes, addr);
    else
        printf("Write %d byte(s) to %llu\n", arg_bytes, addr);
    printf("  tag = %llx, idx = %llx, offset = %llx\n", _tag, _idx, _offset);
#endif
    hit = find_line(_tag, _idx, _line);
    time = 0;

    if (config_.use_bypass == 1 && hit == 0 && is_set_full(_idx))
    {
        int tmpidx;
        bool ret = true;
        for (tmpidx = 0; tmpidx < config_.mct_size; tmpidx++)
            if (mct[_idx][tmpidx] == (unsigned long long)-1 || mct[_idx][tmpidx] == _tag)
            {
                ret = false;
                break;
            }
        if (ret)
        {
            int lower_hit = 0, lower_time = 0;
            time += latency_.bus_latency + latency_.hit_latency;
            lower_->HandleRequest(addr, arg_bytes, read, content, lower_hit, lower_time);
            time += lower_time;
            stats_.miss_time += lower_time;
            stats_.access_time += time;
            stats_.access_counter++;
            stats_.miss_num++;
            stats_.bypass_num++;
            MCT_update(get_idx(addr), get_tag(addr));
            return;
        }
    }

    if (read == 1)  // read
    {
        if (hit == 1)   // read hit
        {
#ifdef DEBUG
            printf("  Read hit!");
#endif
            hit = 1;
            time += latency_.bus_latency + latency_.hit_latency;
            stats_.access_time += latency_.bus_latency + latency_.hit_latency;
            stats_.access_counter++;
            read_hit(addr, _idx, _line, _offset, arg_bytes, content, time);
#ifdef DEBUG
            print();
#endif
            return;
        }
        else    // read miss
        {
#ifdef DEBUG
            printf("  Read miss!");
#endif
            hit = 0;
            stats_.access_counter++;
            stats_.miss_num++;
            time += latency_.bus_latency + latency_.hit_latency;
            read_miss(addr, _idx, _offset, arg_bytes, content, time);
            stats_.access_time += time;
#ifdef DEBUG
            print();
#endif
            return;
        }
    }
    else    // write
    {
        if (hit == 1)   // write hit
        {
#ifdef DEBUG
            printf("  Write hit!");
#endif
            hit = 1;
            time += latency_.bus_latency + latency_.hit_latency;
            stats_.access_counter++;
            write_hit(addr, _idx, _line, _offset, arg_bytes, content, time);
            stats_.access_time += time;
#ifdef DEBUG
            print();
#endif
            return;
        }
        else    // write miss
        {
#ifdef DEBUG
            printf("  Write miss!");
#endif
            hit = 0;
            stats_.access_counter++;
            stats_.miss_num++;
            time += latency_.bus_latency + latency_.hit_latency;
            write_miss(addr, _idx, _offset, arg_bytes, content, time);
            stats_.access_time += time;
#ifdef DEBUG
            print();
#endif
            return;
        }
    }
}

void Cache::LRU_update(int arg_set, int arg_line)
{
    for (int i = 0; i < line_num; i++)
        if (i == arg_line) counter[arg_set][i] = 0;
        else counter[arg_set][i]++;
}

void Cache::PLRU_update(int arg_set, int arg_line)
{
    int node, tmp;
    for (node = 1, tmp = arg_line; node < line_num; )
    {
        bt[arg_set][node] = (((tmp % 2) == 0) ? 1 : 0);
        node = node * 2 + (tmp % 2);
        tmp /= 2;
    }
    //printf("PLRU updated\n");
}

int Cache::PLRU_sel(int arg_set)
{
    int node, res = 0, base;
    for (node = 1, base = 1; node < line_num; base *= 2)
    {
        res += base * bt[arg_set][node];
        node = node * 2 + bt[arg_set][node];
    }
    //printf("PLRU selected %d\n", res);
    return res;
}

bool Cache::is_set_full(int arg_set)
{
    for (int i = 0; i < line_num; i++)
        if (!valid[arg_set][i])
            return false;
    return true;
}

void Cache::MCT_update(int arg_set, int arg_tag)
{
    for (int i = config_.mct_size-1; i > 0; i--)
        mct[arg_set][i] = mct[arg_set][i-1];
    mct[arg_set][0] = arg_tag;
}

void Cache::prefetch(unsigned long long curr_addr)
{
    for (unsigned long long addr = curr_addr + config_.prefetch_stride * line_size;
         addr <= curr_addr + line_size * config_.prefetch_stride * config_.prefetch_num;
         addr += config_.prefetch_stride * line_size)
    {
    int already_in_cache = -1;
    find_line(get_tag(addr), get_idx(addr), already_in_cache);
    if (already_in_cache != -1) // already in cache, don't prefetch
        continue;
    /*
    int tmpidx;
    bool ret = true;
    int _idx = get_idx(curr_addr), _tag = get_tag(curr_addr);
    for (tmpidx = 0; tmpidx < config_.mct_size; tmpidx++)
        if (mct[_idx][tmpidx] == (unsigned long long)-1 || mct[_idx][tmpidx] == _tag)
        {
            ret = false;
            break;
        }
    if (ret)
        continue;
        */

    stats_.prefetch_num++;
    int arg_set = get_idx(addr);
    int sub_line = -1, max_cnt = 0;
    bool found = false;

    for (int i = 0; i < line_num; i++)
        if (!valid[arg_set][i]) // empty line found
        {
            found = true;
            sub_line = i;
            break;
        }
        else if (config_.replace_policy == REPLACE_LRU
                 && max_cnt < counter[arg_set][i]) // LRU line found
        {
            max_cnt = counter[arg_set][i];
            sub_line = i;
        }
    if (config_.replace_policy == REPLACE_RANDOM && !found)
        sub_line = rand() % line_num;
    else if (config_.replace_policy == REPLACE_PLRU && !found)
        sub_line = PLRU_sel(arg_set);

    if (!found) // replace
    {
        if (config_.use_bypass == 1)
            MCT_update(arg_set, get_tag(addr));
        if (config_.write_through == 0 && dirty[arg_set][sub_line])   // write-back
        {
            int wb_hit = 0, wb_time = 0;
            lower_->HandleRequest(((tag[arg_set][sub_line] << idx_bits) | arg_set) << offset_bits,
                                  1 << offset_bits, 0, bytes[arg_set][sub_line], wb_hit, wb_time);
        }
    }

    valid[arg_set][sub_line] = true;
    dirty[arg_set][sub_line] = false;
    tag[arg_set][sub_line] = get_tag(addr);
    stats_.prefetch_num++;

    int lower_hit = 0, lower_time = 0;
    lower_->HandleRequest((addr >> offset_bits) << offset_bits,
                          1 << offset_bits, 1, bytes[arg_set][sub_line], lower_hit, lower_time);
    }
}
