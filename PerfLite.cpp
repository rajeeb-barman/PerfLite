#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include<vector>

struct ReadFormat
{
    uint64_t m_value;
    uint64_t m_enabled;
    uint64_t m_running;
};

static long EventConfig(struct perf_event_attr *hw_event,
                        pid_t pid,
                        int cpu,
                        int group_fd,
                        unsigned long flags)
{
    int ret;
    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
    return ret;
}

int ConfigurePmc(std::vector<int> &fdList, std::vector<uint64_t> evList)
{
    int ret = 0;
    int groupFd = -1;
    struct perf_event_attr event_attr;
    memset(&event_attr, 0, sizeof(event_attr));
    uint64_t flags = PERF_FLAG_FD_CLOEXEC;
    event_attr.type = PERF_TYPE_RAW;
    event_attr.size = sizeof(event_attr);
    event_attr.disabled = 1;
    event_attr.exclude_kernel = 1;
    event_attr.exclude_hv = 1;
    event_attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

    for (auto Iter:evList)
    {
        int fd = -1;
        event_attr.config = Iter;

        // Configure the events here
        fd = EventConfig(&event_attr, 0, -1, groupFd, 0); 

        if (fd == -1)
        {
            fprintf(stderr, "Error: Config failed %llx\n", event_attr.config);
            ret = -1;
            break;
        }
        fdList.push_back(fd);

        if (groupFd == -1)
        {
            groupFd = fd;
        }
    }

    return ret;
}

void StartMonitoring(std::vector<int> &fdList)
{
    for (auto Iter:fdList)
    {
        ioctl(Iter, PERF_EVENT_IOC_RESET);
        ioctl(Iter, PERF_EVENT_IOC_ENABLE);
    }
}

void PrintData(std::vector<ReadFormat> dataList, std::vector<uint64_t> evList)
{
    uint32_t idx = 0;

    for (auto Iter:dataList)
    {
        std::cout << "Value[0x"<< std::hex << evList.at(idx)<<"]-" << std::dec
                  << Iter.m_value << "\tEnabled: " << Iter.m_enabled << "\tRun: "
                  << Iter.m_running << std::endl;
        idx++;
    }
}

void ReadData(std::vector<int> &fdList, std::vector<ReadFormat>& dataList)
{
    uint32_t readSize = 0;

    for (auto Iter:fdList)
    {
        ioctl(fdList.at(0), PERF_EVENT_IOC_DISABLE);
        break;
    }

    for (auto Iter:fdList)
    {
        ReadFormat data;
        unsigned long long counter;
        readSize = read(Iter, &data, sizeof(ReadFormat));
        close(Iter);

        if (readSize == sizeof(ReadFormat))
        {
            dataList.push_back(data);
        }
    }
}

int main(int argc, char *argv[])
{
    int ret = 0;
    std::vector<int> fdList;
    std::vector<ReadFormat> dataList;
    std::vector<uint64_t> evList;

    // Events are based on AMD PPR
    uint64_t ev[] = {0x430076, 0x4300C0, 0x4300C2, 0x4300C3, 0x4300C4, 0x4300C5};
    uint32_t len = sizeof(ev)/sizeof(uint64_t);

    for (int idx = 0; idx < len; idx++)
    {
        evList.push_back(ev[idx]);
    }

    ret = ConfigurePmc(fdList, evList);

    if(ret != 0)
    {
        std::cout << "Error: Failed configuration" << std::endl;
    }

    if (!ret)
    {
        StartMonitoring(fdList);
        std::cout << "Profile started....\nPress <enter> to stop" << std::endl;

        // Do something
        getchar();
        ReadData(fdList, dataList);
        PrintData(dataList, evList);
    }
}
