#ifndef __TRANSACTION_GENERATOR_H
#define __TRANSACTION_GENERATOR_H

#include <sys/mman.h>
#include <time.h>
#include <stdlib.h>
#include <string>
#include <cstdint>
#include "./configuration.h"
#include "./common.h"
#include "./pim_config.h"
#include "./half.hpp"

#define EVEN_BANK 0
#define ODD_BANK  1

#define NUM_WORD_PER_ROW     32
#define NUM_CHANNEL          16
#define NUM_BANK_PER_CHANNEL 16
#define NUM_BANK             (NUM_BANK_PER_CHANNEL * NUM_CHANNEL)
#define SIZE_WORD            32
#define SIZE_ROW             (SIZE_WORD * NUM_WORD_PER_ROW)

#define MAP_SBMR             0x3fff
#define MAP_ABMR             0x3ffe
#define MAP_PIM_OP_MODE      0x3ffd
#define MAP_CRF              0x3ffc
#define MAP_GRF              0x3ffb
#define MAP_SRF              0x3ffa

#define C_NORMAL "\033[0m"
#define C_RED    "\033[031m"
#define C_GREEN  "\033[032m"
#define C_YELLOW "\033[033m"
#define C_BLUE   "\033[034m"

struct Address {
    Address()
        : channel(-1), rank(-1), bankgroup(-1), bank(-1), row(-1), column(-1) {}
    Address(int channel, int rank, int bankgroup, int bank, int row, int column)
        : channel(channel),
          rank(rank),
          bankgroup(bankgroup),
          bank(bank),
          row(row),
          column(column) {}
    Address(const Address& addr)
        : channel(addr.channel),
          rank(addr.rank),
          bankgroup(addr.bankgroup),
          bank(addr.bank),
          row(addr.row),
          column(addr.column) {}
    int channel;
    int rank;
    int bankgroup;
    int bank;
    int row;
    int column;
}

class TransactionGenerator {
 public:
    TransactionGenerator() {
        fd_ = open("/dev/PIM", O_RDWR|O_SYNC);
        if (fd_ < 0)
        {
            printf("open fail %s\n", strerror(errno));
            exit(1);
        }
        printf("fd %d\n", fd_);

        pmemAddr_ = (uint8_t *) mmap(NULL, LEN_PIM,
                                     PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE,
                                     fd, 0);
        if (pmemAddr_ == (uint8_t*) MAP_FAILED)
            perror("mmap");

        printf("finish mmap at %llx\n", pmemAddr_);

        burstSize_ = 32;

        data_temp_ = (uint8_t *) malloc(burstSize_);

        ch_pos_ = 0;
        ba_pos_ = ch_pos_ + 4;
        bg_pos_ = ba_pos_ + 4;
        co_pos_ = bg_pos_ + 0;
        ra_pos_ = co_pos_ + 5;
        shift_bits_ = 5;
    }
    ~TransactionGenerator() {
        munmap(pmemAddr_, LEN_PIM);
        close(fd_);
    }
    // virtual void ClockTick() = 0;
    virtual void Initialize() = 0;
    virtual void SetData() = 0;
    virtual void Execute() = 0;
    virtual void GetResult() = 0;
    virtual void CheckResult() = 0;

    uint64_t ReverseAddressMapping(Address& addr);
    uint64_t Ceiling(uint64_t num, uint64_t stride);
    void TryAddTransaction(uint64_t hex_addr, bool is_write, uint8_t *DataPtr);
    void Barrier();

 protected:
    int fd_;
    uint8_t *pmemAddr_;
    unsigned int burstSize_;

    uint8_t data_temp_[32];
    uint8_t write_data_[32];

    int ch_pos_;
    int ra_pos_;
    int bg_pos_;
    int ba_pos_;
    int ro_pos_;
    int co_pos_;
    int shift_bits_;
};

class AddTransactionGenerator : public TransactionGenerator {
 public:
    AddTransactionGenerator(uint64_t n,
                            uint8_t *x,
                            uint8_t *y,
                            uint8_t *z)
        : n_(n), x_(x), y_(y), z_(z) {}
    void Initialize() override;
    void SetData() override;
    void Execute() override;
    void GetResult() override;
    void CheckResult() override;

 private:
    uint8_t *x_, *y_, *z_;
    uint64_t n_;
    uint64_t addr_x_, addr_y_;
    uint64_t ukernel_access_size_;
    uint64_t ukernel_count_per_pim_;
    uint32_t *ukernel_;
};


class GemvTransactionGenerator : public TransactionGenerator {
 public:
    GemvTransactionGenerator(uint64_t m,
                             uint64_t n,
                             uint8_t *A,
                             uint8_t *x,
                             uint8_t *y)
        : m_(m), n_(n), A_(A), x_(x), y_(y) {}
    void Initialize() override;
    void SetData() override;
    void Execute() override;
    void GetResult() override;
    void CheckResult() override;

 private:
    void ExecuteBank(int bank);

    uint8_t *A_, *x_, *y_;
    uint8_t *A_T_;
    uint64_t m_, n_;
    uint64_t addr_A_, addr_y_;
    uint64_t ukernel_access_size_;
    uint64_t ukernel_count_per_pim_;
    uint32_t *ukernel_gemv_;
    uint32_t *ukernel_reduce_;
};

#endif // __TRANSACTION_GENERATOR_H
