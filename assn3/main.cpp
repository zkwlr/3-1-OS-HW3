#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <vector>
#include <queue>
#include <cassert>
#include <climits>
using namespace std;

struct Page
{
    int pid = -1;         // process id, -1: 비어있는 page
    int page_number = -1; // Page id, -1: 비어있는 page
    int frame_number;
    char R_W_bit = 'W'; // 기본적으로 할당된 페이지는 읽기 권한과 쓰기 권한을 모두 가짐
    int alloc_id = -1;  // allocate 명령에서 할당된 page들을 구분하기 위한 id
    // 교체 알고리즘 구현용 변수
    int refercycle = 0; // page가 마지막으로 참조된 cycle, LRU에서 사용
    int refercount = 1; // page가 참조된 횟수, 물리 메모리에 할당 순간 1로 초기화, LFU, MFU에서 사용
};

struct Process
{
    string name;                // 프로세스 이름
    int pid;                    // 프로세스 ID
    int ppid;                   // 부모 프로세스 ID
    char waiting_type;          // waiting일 때 S, W중 하나
    int processcount = 1;       // 프로세스 생성할 때마다 1씩 증가
    Page *emtpypage = new Page; // 빈 page ptr
    vector<string> lines;
    vector<Page *> virtual_memory; // 32bit 크기의 가상 메모리
    // 1이면 index 출력, 0이면 - 출력
    vector<Page *> page_table; // 페어 자료형(frame number, R/W bit)으로 구현하기
    // 배열 index: virtaul mem의 page number, 배열 값: physical mem의 frame number
    Process() : virtual_memory(32, emtpypage), page_table(32, emtpypage) {} // 빈(pid -1) page로 초기화
    int alloc_id = -1;
    int alloc_count = 0; // Page id를 주기 위한 변수, alloc할때 size만큼 여기 더해준다, 자식 프로세스가 물려받는다.(페이지가 복사되기 때문에 페이지ID도 같은 숫자만큼 할당되어 있음)
};

struct Status
{
    int cycle;
    string mode;
    string command;
    Process *process_running;
    Page *emptypage = new Page; // 빈 page ptr
    queue<Process *> process_ready;
    queue<Process *> process_waiting;
    Process *process_new;
    Process *process_terminated;
    vector<Page *> physical_memory; // 16개의 frame으로 구성된 physical memory
    int replaceindex = 0;           // FIFO 알고리즘에서 교체할 frame의 index를 가리키는 포인터, 교체되어 나갈 때마다 +1 해준다
    Status() : physical_memory(16, emptypage) {}
};

// 함수

void print_status(Status &status)
{
    // 0. 몇번째 cycle인지
    printf("[cycle #%d]\n", status.cycle);

    // 1. 현재 실행 모드 (user or kernel)
    printf("1. mode: %s\n", status.mode.c_str());

    // 2. 현재 실행 명령어
    printf("2. command: %s\n", status.command.c_str());

    // 3. 현재 실행중인 프로세스의 정보. 없을 시 none 출력
    if (status.process_running == NULL)
    {
        printf("3. running: none\n");
    }
    else
    {
        Process *i = status.process_running;
        printf("3. running: %d(%s, %d)\n", i->pid, i->name.c_str(), i->ppid);
    }

    // 4. physical memory의 상태
    printf("4. physical memory:\n");
    for (int i = 0; i < status.physical_memory.size(); i += 4) // | 문자를 i가 4의 배수일 때마다 출력
    {
        printf("|");
        for (int j = i; j < i + 4; j++) // j가 i의 역할을 대신함, j = 0, 1, ...,15
        {
            if (j < status.physical_memory.size())
            {
                if (status.physical_memory[j]->pid == -1) // pid가 -1이면 비어있음(- 출력)
                {
                    printf("-");
                }
                else // pid가 -1이 아니면 frame에 저장된 page의 pid와 page number 출력
                {
                    printf("%d(%d)", status.physical_memory[j]->pid, status.physical_memory[j]->page_number);
                }
                if (j != i + 3) // 4개 간격으로 구분하므로 4의 배수번째 메모리를 출력할 때는 공백 넣지 않음
                {
                    printf(" ");
                }
            }
            else
            {
                continue;
            }
        }
    }
    printf("|\n");

    // 5. virtual memory의 상태, process_running = none 이면 출력 X
    if (status.process_running != NULL)
    {
        printf("5. virtual memory:\n");
        for (int i = 0; i < status.process_running->virtual_memory.size(); i += 4)
        {
            printf("|");
            for (int j = i; j < i + 4; j++)
            {
                if (j < status.process_running->virtual_memory.size())
                {
                    if (status.process_running->virtual_memory[j]->pid == -1)
                    {
                        printf("-");
                    }
                    else
                    {
                        printf("%d", status.process_running->virtual_memory[j]->page_number);
                    }
                    if (j != i + 3)
                    {
                        printf(" ");
                    }
                }
                else
                {
                    continue;
                }
            }
        }
        printf("|\n");
    }

    // 6. page table의 상태, process_running = none 이면 출력 X
    if (status.process_running != NULL)
    {
        printf("6. page table:\n"); // page table에서 page id에 해당하는 frame number 출력
        for (int i = 0; i < status.process_running->virtual_memory.size(); i += 4)
        {
            printf("|");
            for (int j = i; j < i + 4; j++)
            {
                if (j < status.process_running->virtual_memory.size())
                {
                    if (status.process_running->virtual_memory[j]->pid == -1              // frame이 비어있거나
                        || status.process_running->virtual_memory[j]->frame_number == -1) // page가 있지만 frame에 할당되지 않은경우(물리 메모리에 없는 경우)
                    {
                        printf("-");
                    }
                    else
                    {
                        printf("%d", status.process_running->page_table[j]->frame_number);
                    }
                    if (j != i + 3)
                    {
                        printf(" ");
                    }
                }
                else
                {
                    continue;
                }
            }
        }
        printf("|\n");
        for (int i = 0; i < status.process_running->virtual_memory.size(); i += 4) // page의 읽기/쓰기 권한 출력
        {
            printf("|");
            for (int j = i; j < i + 4; j++)
            {
                if (j < status.process_running->virtual_memory.size())
                {
                    if (status.process_running->virtual_memory[j]->pid == -1)
                    {
                        printf("-");
                    }
                    else
                    {
                        printf("%c", status.process_running->page_table[j]->R_W_bit);
                    }
                    if (j != i + 3)
                    {
                        printf(" ");
                    }
                }
                else
                {
                    continue;
                }
            }
        }
        printf("|\n");
    }

    // 매 cycle 간의 정보는 두번의 개행으로 구분
    printf("\n");

    status.cycle++;
    // // 4. 현재 ready 상태인 프로세스의 정보. 왼쪽에 있을 수록 먼저 queue에 들어온 프로세스이다.
    // if (status.process_ready.size() == 0)
    // {
    //     printf("4. ready: none\n");
    // }
    // else
    // {
    //     printf("4. ready:");
    //     for (int i = 0; i < status.process_ready.size(); ++i)
    //     { // 공백 한칸으로 구분
    //         printf(" %d", status.process_ready.front()->pid);
    //         status.process_ready.push(status.process_ready.front());
    //         status.process_ready.pop();
    //     }
    //     printf("\n");
    // }

    // // 5. 현재 waiting 상태인 프로세스의 정보. 왼쪽에 있을 수록 먼저 waiting이 된 프로세스이다.
    // if (status.process_waiting.size() == 0)
    // {
    //     printf("5. waiting: none\n");
    // }
    // else
    // {
    //     printf("5. waiting:");
    //     for (int i = 0; i < status.process_waiting.size(); ++i)
    //     { // 공백 한칸으로 구분
    //         printf(" %d(%c)", status.process_waiting.front()->pid, status.process_waiting.front()->waiting_type);
    //         status.process_waiting.push(status.process_waiting.front());
    //         status.process_waiting.pop();
    //     }
    //     printf("\n");
    // }

    // // 6. New 상태의 프로세스
    // if (status.process_new == NULL)
    // {
    //     printf("6. new: none\n");
    // }
    // else
    // {
    //     Process *i = status.process_new;
    //     printf("6. new: %d(%s, %d)\n", i->pid, i->name.c_str(), i->ppid);
    // }

    // // 7. Terminated 상태의 프로세스
    // if (status.process_terminated == NULL)
    // {
    //     printf("7. terminated: none\n");
    // }
    // else
    // {
    //     Process *i = status.process_terminated;
    //     printf("7. terminated: %d(%s, %d)\n", i->pid, i->name.c_str(), i->ppid);
    // }

    // // 매 cycle 간의 정보는 두번의 개행으로 구분
    // printf("\n");

    // status.cycle++;
}

void open_stream_to_file(Status &status, const string &filename)
{
    // 출력 스트림을 콘솔에서 파일로 바꾸는 함수
    // 쓸 파일 이름 지정후 있으면 맨 뒤에서 append 모드로 이어서 기록
    // 없으면 result 파일 만듬

    // 파일이 안만들어지거나 안 열리면 에러
    FILE *fp = fopen(filename.c_str(), "a");
    if (fp == NULL)
    {
        printf("Error: could not open file");
        return;
    }
    // 표준 출력 스트림을 파일로 리다이렉트
    freopen(filename.c_str(), "a", stdout);
}

vector<string> read_file(string filename, string &directory, string &currentpath)
{
    // 작업 디렉터리를 실행할 가상 프로그램이 있는 입력받은 경로로 변경
    std::filesystem::current_path(directory);
    // 파일 라인을 저장할 벡터
    vector<string> filelines;
    ifstream file(filename); // 읽을 파일 열기
    if (file.is_open())
    {
        string line;
        while (getline(file, line)) // 파일의 끝까지 한 줄씩 읽기
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.erase(line.length() - 1);
            }
            filelines.push_back(line); // 읽은 라인을 벡터에 저장
            // cout << line << endl;      // 읽은 라인 출력
        }
        file.close(); // 파일 닫기
    }
    else
    {
        cerr << "Error: could not open file " << filename << endl;
    }
    // 작업 디렉터리를 result를 저장할 경로로 변경
    std::filesystem::current_path(currentpath);
    return filelines;
}

// 파일을 한 줄씩 읽어서, 공백 단위로 분리한 뒤, 결과 배열에 저장하는 함수
vector<string> read_word(Process *p)
{

    stringstream ss(p->lines.front()); // 공백 분리 결과를 저장할 배열

    vector<string> words;
    string word;

    while (getline(ss, word, ' '))
    {
        words.push_back(word);
    } // 스트림을 한 줄씩 읽어, 공백 단위로 분리한 뒤, 결과 배열에 저장
    return words;
}

Process *make_process(Process *p, string &directory, string &currentpath)
{
    p->processcount++; // make_process 명령이 몇번 실행되었는지 init에 기록
    Process *newP = new Process;
    vector<string> words = read_word(p);
    newP->name = words[1];
    newP->pid = p->processcount; // init 프로세스가 생성한 자식 프로세스의 pid = 마지막 생성 pid + 1
    newP->ppid = p->pid;
    newP->waiting_type = p->waiting_type;
    newP->lines = read_file(words[1], directory, currentpath);
    newP->alloc_id = p->alloc_id;
    newP->alloc_count = p->alloc_count; // 페이지 id가 얼마만큼 할당되었는지도 복사
    for (int i = 0; i < p->virtual_memory.size(); i++)
    {
        if (p->virtual_memory[i]->pid != -1)
        { // page가 있을 경우
            p->virtual_memory[i]->R_W_bit = 'R';
        } // 복사하기 전 공유 page 읽기 권한 R로 변경
    }

    for (int i = 0; i < newP->virtual_memory.size(); i++)
    {
        if (p->virtual_memory[i]->pid != -1)
        { // 부모 process의 page가 있을 경우
            Page *page = new Page;
            page->pid = newP->pid;
            page->page_number = p->virtual_memory[i]->page_number;
            page->frame_number = p->virtual_memory[i]->frame_number;
            page->R_W_bit = p->virtual_memory[i]->R_W_bit;
            page->alloc_id = p->alloc_id;                        // 할당할 페이지에 alloc_id 기록
            page->refercycle = p->virtual_memory[i]->refercycle; // 참조 사이클 기록
            newP->virtual_memory[i] = page;                      // 복사한 페이지 포인터 자식 프로세스 가상 메모리에 저장
            newP->page_table[i] = newP->virtual_memory[i];       // 부모 프로세스의 page table을 자식 프로세스에 복사
        }
    }
    p->lines.erase(p->lines.begin()); // 프로세스 생성 후, 원래 프로세스의 첫 줄 삭제
    return newP;
}

void sleep_list_update(Status &status, vector<pair<int, int>> &sleep_list)
{
    // S 인 프로세스 업데이트
    for (int i = 0; i < sleep_list.size(); i++)
    { // sleep_list의 저장된 프로세스의 사이클과 현재 사이클이 같으면 레디 큐에 넣는다.
        if (sleep_list[i].second == status.cycle)
        {
            for (int j = 0; j < status.process_waiting.size(); j++)
            {
                if (status.process_waiting.front()->pid == sleep_list[i].first)
                {
                    status.process_ready.push(status.process_waiting.front());
                    status.process_waiting.pop();
                }
                else
                {
                    status.process_waiting.push(status.process_waiting.front());
                    status.process_waiting.pop();
                }
            }
            // status.process_ready.push(status.process_waiting.front());
            // status.process_waiting.pop();
            // wait 호출해서 대기중인 게 있을수도 있어서, 이렇게 뽑아내면 안된다.
        }
    }
    // W 인 프로세스 업데이트
    // ready, waiting, running 에 있는 프로세스를 다 뽑아서, waiting의 pid와 pcheck_list의 ppid가 같은지 확인하고, 같으면 레디 큐에 넣는다.
    vector<Process *> pcheck_list;
    for (int i = 0; i < status.process_ready.size(); i++)
    {
        pcheck_list.push_back(status.process_ready.front());
        status.process_ready.push(status.process_ready.front());
        status.process_ready.pop();
    }
    for (int i = 0; i < status.process_waiting.size(); i++)
    {
        pcheck_list.push_back(status.process_waiting.front());
        status.process_waiting.push(status.process_waiting.front());
        status.process_waiting.pop();
    }
    if (status.process_running != NULL)
    {
        pcheck_list.push_back(status.process_running);
    }
    for (int i = 0; i < status.process_waiting.size(); i++)
    { // pcheck_list의 ppid와 waiting의 pid가 같은 게 없으면 ready 큐에 넣는다.
        if (status.process_waiting.front()->waiting_type == 'W')
        {
            bool check = false; // true 면 자식프로세스가 있다. false면 없고 레디큐에 넣는다.
            for (int j = 0; j < pcheck_list.size(); j++)
            {
                if (status.process_waiting.front()->pid == pcheck_list[j]->ppid)
                {
                    check = true;
                    break;
                }
            }
            if (check == false) // 레디큐에 넣기
            {
                status.process_ready.push(status.process_waiting.front());
                status.process_waiting.pop();
            }
            else // waiting type이 W지만 자식프로세스가 있으므로 다음 waiting 프로세스를 검사한다.
            {
                status.process_waiting.push(status.process_waiting.front());
                status.process_waiting.pop();
            }
        }
        else // waiting type이 s면 그냥 다음 waiting 프로세스를 검사한다.
        {
            status.process_waiting.push(status.process_waiting.front());
            status.process_waiting.pop();
        }
    }
}

void run_list_update(Status &status, vector<pair<int, int>> &run_list)
{
    for (int i = 0; i < run_list.size(); i++)
    { // run_list의 저장된 프로세스의 사이클과 현재 사이클이 같으면 run 커맨드를 종료한다.
        if (run_list[i].second == status.cycle)
        {
            status.command = "none";
        }
    }
}

void schedule(Status *status)
{
    status->command = "schedule";
    status->mode = "kernel";
    if (status->process_ready.size() != 0)
    {
        status->process_running = status->process_ready.front();
        status->process_ready.pop();
    }

    print_status(*status);
    status->mode = "user";
    status->command = "none";
}

void idle(Status *status)
{
    status->command = "idle";
    status->mode = "kernel";
    print_status(*status);
    status->mode = "user";
    status->command = "none";
}

// 페이지 교체 알고리즘, 빼기만 함
void lru(Status *status) // 가장 참조된 지 오래된 page를 교체
{
    // 교체되야 할 페이지 물리메모리에서 찾기
    Page *page_to_replace = nullptr;
    int replaceindex = 0; // 교체되어야 할 페이지의 물리 메모리 index
    for (int i = 0; i < status->physical_memory.size(); i++)
    {
        if (status->physical_memory[i]->pid != -1)
        { // 일단 빈 페이지는 넘긴다.
            if (page_to_replace == nullptr || page_to_replace->refercycle > status->physical_memory[i]->refercycle)
            {
                page_to_replace = status->physical_memory[i];
                replaceindex = i;
            }
        }
        else
        {
            continue;
        }
    }

    vector<Process *> temp; // running 프로세스와 ready, waiting queue에 있는 프로세스들을 임시로 저장할 벡터
    temp.push_back(status->process_running);
    for (int i = 0; i < status->process_ready.size(); i++)
    {
        temp.push_back(status->process_ready.front());
        status->process_ready.push(status->process_ready.front());
        status->process_ready.pop();
    }
    for (int i = 0; i < status->process_waiting.size(); i++)
    {
        temp.push_back(status->process_waiting.front());
        status->process_waiting.push(status->process_waiting.front());
        status->process_waiting.pop();
    }

    for (Process *p : temp)
    {
        for (Page *page : p->virtual_memory)
        { // 모든 프로세스의 가상 메모리의 페이지를 돌면서 pid와 page_number가 빼낼 물리 메모리의 페이지와 같으면 테이블에서 매핑을 삭제한다.
            // 만약 그 페이지가 공유중인 페이지라면(R), 공유중인 프로세스의 page table에서도 매핑을 제거해야 한다.
            if (page->pid == status->physical_memory[replaceindex]->pid && page->page_number == status->physical_memory[replaceindex]->page_number)
            {
                page->frame_number = -1;
                if (page->R_W_bit == 'R')
                {
                    for (Process *shareprocess : temp)
                    {
                        // 공유중인 프로세스의 페이지 중 R인 것들도 매핑 해제
                        for (Page *sharepage : shareprocess->virtual_memory)
                        {
                            if (shareprocess->pid != page->pid && sharepage->page_number == page->page_number && sharepage->R_W_bit == 'R')
                            {
                                sharepage->frame_number = -1;
                            }
                        }
                    }
                }
            }
        }
    }
    // 교체되어야할 page가 있는 frame을 빈 페이지로 변경한다.
    status->physical_memory[replaceindex] = status->emptypage;
}
void fifo(Status *status)
{
    vector<Process *> temp; // running 프로세스와 ready, waiting queue에 있는 프로세스들을 임시로 저장할 벡터
    temp.push_back(status->process_running);
    for (int i = 0; i < status->process_ready.size(); i++)
    {
        temp.push_back(status->process_ready.front());
        status->process_ready.push(status->process_ready.front());
        status->process_ready.pop();
    }
    for (int i = 0; i < status->process_waiting.size(); i++)
    {
        temp.push_back(status->process_waiting.front());
        status->process_waiting.push(status->process_waiting.front());
        status->process_waiting.pop();
    }
    for (Process *p : temp)
    {
        for (Page *page : p->virtual_memory)
        { // 모든 프로세스의 가상 메모리의 페이지를 돌면서 pid와 page_number가 빼낼 물리 메모리의 페이지와 같으면 테이블에서 매핑을 삭제한다.
            // 만약 그 페이지가 공유중인 페이지라면(R), 공유중인 프로세스의 page table에서도 매핑을 제거해야 한다.
            if (page->pid == status->physical_memory[status->replaceindex]->pid && page->page_number == status->physical_memory[status->replaceindex]->page_number)
            {
                page->frame_number = -1;
                if (page->R_W_bit == 'R')
                {
                    for (Process *shareprocess : temp)
                    {
                        // 공유중인 프로세스의 페이지 중 R인 것들도 매핑 해제
                        for (Page *sharepage : shareprocess->virtual_memory)
                        {
                            if (shareprocess->pid != page->pid && sharepage->page_number == page->page_number && sharepage->R_W_bit == 'R')
                            {
                                sharepage->frame_number = -1;
                            }
                        }
                    }
                }
            }
        }
    }
    // 교체되어야할 page가 있는 frame을 빈 페이지로 변경한다.
    status->physical_memory[status->replaceindex] = status->emptypage;
    // physical memory 에서 다음으로 나갈 page 지정, 16 이상 커지면 0으로 초기화
    status->replaceindex++;
    status->replaceindex = status->replaceindex % 16;
}
void lfu(Status *status)
{
    // 교체되야 할 페이지 물리메모리에서 찾기
    Page *page_to_replace = nullptr;
    int replaceindex = 0; // 교체되어야 할 페이지의 물리 메모리 index
    int min = INT_MAX;    // 교체되어야 할 페이지의 reference count, 초기에 최대 정수값으로 설정
    for (int i = 0; i < status->physical_memory.size(); i++)
    {
        if (status->physical_memory[i]->pid != -1)
        {
            // 일단 빈 페이지는 넘긴다.
            if (status->physical_memory[i]->refercount < min)
            {
                // 참조횟수가 가장 적고 주소가 제일 작은 페이지를 찾는다.
                min = status->physical_memory[i]->refercount;
                page_to_replace = status->physical_memory[i];
                replaceindex = i;
            }
        }
        else
        {
            continue;
        }
    }

    vector<Process *> temp; // running 프로세스와 ready, waiting queue에 있는 프로세스들을 임시로 저장할 벡터
    temp.push_back(status->process_running);
    for (int i = 0; i < status->process_ready.size(); i++)
    {
        temp.push_back(status->process_ready.front());
        status->process_ready.push(status->process_ready.front());
        status->process_ready.pop();
    }
    for (int i = 0; i < status->process_waiting.size(); i++)
    {
        temp.push_back(status->process_waiting.front());
        status->process_waiting.push(status->process_waiting.front());
        status->process_waiting.pop();
    }

    for (Process *p : temp)
    {
        for (Page *page : p->virtual_memory)
        { // 모든 프로세스의 가상 메모리의 페이지를 돌면서 pid와 page_number가 빼낼 물리 메모리의 페이지와 같으면 테이블에서 매핑을 삭제한다.
            // 만약 그 페이지가 공유중인 페이지라면(R), 공유중인 프로세스의 page table에서도 매핑을 제거해야 한다.
            if (page->pid == status->physical_memory[replaceindex]->pid && page->page_number == status->physical_memory[replaceindex]->page_number)
            {
                page->frame_number = -1;
                if (page->R_W_bit == 'R')
                {
                    for (Process *shareprocess : temp)
                    {
                        // 공유중인 프로세스의 페이지 중 R인 것들도 매핑 해제
                        for (Page *sharepage : shareprocess->virtual_memory)
                        {
                            if (shareprocess->pid != page->pid && sharepage->page_number == page->page_number && sharepage->R_W_bit == 'R')
                            {
                                sharepage->frame_number = -1;
                            }
                        }
                    }
                }
            }
        }
    }
    // 교체되어야할 page가 있는 frame을 빈 페이지로 변경한다.
    status->physical_memory[replaceindex] = status->emptypage;
}
void mfu(Status *status)
{
    // 교체되야 할 페이지 물리메모리에서 찾기
    Page *page_to_replace = nullptr;
    int replaceindex = 0; // 교체되어야 할 페이지의 물리 메모리 index
    int max = 0;          // 교체되어야 할 페이지의 reference count, 초기에 0으로 설정
    for (int i = 0; i < status->physical_memory.size(); i++)
    {
        if (status->physical_memory[i]->pid != -1)
        {
            // 일단 빈 페이지는 넘긴다.
            if (status->physical_memory[i]->refercount > max)
            {
                // 참조횟수가 가장 많고 주소가 제일 작은 페이지를 찾는다.
                max = status->physical_memory[i]->refercount;
                page_to_replace = status->physical_memory[i];
                replaceindex = i;
            }
        }
        else
        {
            continue;
        }
    }

    vector<Process *> temp; // running 프로세스와 ready, waiting queue에 있는 프로세스들을 임시로 저장할 벡터
    temp.push_back(status->process_running);
    for (int i = 0; i < status->process_ready.size(); i++)
    {
        temp.push_back(status->process_ready.front());
        status->process_ready.push(status->process_ready.front());
        status->process_ready.pop();
    }
    for (int i = 0; i < status->process_waiting.size(); i++)
    {
        temp.push_back(status->process_waiting.front());
        status->process_waiting.push(status->process_waiting.front());
        status->process_waiting.pop();
    }

    for (Process *p : temp)
    {
        for (Page *page : p->virtual_memory)
        { // 모든 프로세스의 가상 메모리의 페이지를 돌면서 pid와 page_number가 빼낼 물리 메모리의 페이지와 같으면 테이블에서 매핑을 삭제한다.
            // 만약 그 페이지가 공유중인 페이지라면(R), 공유중인 프로세스의 page table에서도 매핑을 제거해야 한다.
            if (page->pid == status->physical_memory[replaceindex]->pid && page->page_number == status->physical_memory[replaceindex]->page_number)
            {
                page->frame_number = -1;
                if (page->R_W_bit == 'R')
                {
                    for (Process *shareprocess : temp)
                    {
                        // 공유중인 프로세스의 페이지 중 R인 것들도 매핑 해제
                        for (Page *sharepage : shareprocess->virtual_memory)
                        {
                            if (shareprocess->pid != page->pid && sharepage->page_number == page->page_number && sharepage->R_W_bit == 'R')
                            {
                                sharepage->frame_number = -1;
                            }
                        }
                    }
                }
            }
        }
    }
    // 교체되어야할 page가 있는 frame을 빈 페이지로 변경한다.
    status->physical_memory[replaceindex] = status->emptypage;
}

// 입력받은 알고리즘에 따라 page를 swap하는 함수
void change_page(const string algorithm, Status *status, const int inputpageidindex)
{
    int pagecount = 0;
    for (Page *page : status->physical_memory) // 현재 물리메모리에 들어있는 page 갯수 세기
    {
        if (page->pid != -1)
            pagecount++;
    }
    if (pagecount == status->physical_memory.size()) // 물리메모리가 가득 차있으면
    {
        // 알고리즘에 따라 교체되어야할 page를 찾고 뺀다.
        if (algorithm == "lru")
        {
            lru(status);
        }
        else if (algorithm == "fifo")
        {
            fifo(status);
        }
        else if (algorithm == "lfu")
        {
            lfu(status);
        }
        else if (algorithm == "mfu")
        {
            mfu(status);
        }
        else
        {
            cout << "ERROR : Invalid page swap algorithm input" << endl;
            assert(algorithm == "lru" || algorithm == "fifo" || algorithm == "lfu" || algorithm == "mfu"); // assert 실패로 프로그램 종료
        }
    } // 빈 자리가 있으면 그냥 진행
    // 그 다음 physical memory의 빈 자리에 page를 넣는다.
    for (int i = 0; i < status->physical_memory.size(); i++)
    { // 자식 프로세스의 명령이고 R인 페이지일때
        if (status->process_running->pid != 1 && status->process_running->page_table[inputpageidindex]->R_W_bit == 'R')
        {
            if (status->physical_memory[i]->pid == -1)
            {
                // 부모 함수는 waiting 큐에 들어가 있으므로 그곳으로 접근해 부모의 페이지를 찾아 물리 메모리에 넣고 table에 매핑
                if (!status->process_waiting.empty())
                {
                    status->process_waiting.front()->virtual_memory[inputpageidindex]->frame_number = i;
                    // 참조된 사이클, 새로 물리메모리에 할당되므로 참조횟수 1로 초기화
                    status->process_waiting.front()->virtual_memory[inputpageidindex]->refercycle = status->cycle;
                    status->process_waiting.front()->virtual_memory[inputpageidindex]->refercount = 1; // 참조횟수 초기화
                    status->physical_memory[i] = status->process_waiting.front()->virtual_memory[inputpageidindex];
                    status->process_waiting.front()->page_table[inputpageidindex]->frame_number = i;
                    // 공유중인 자식 페이지의 page table에도 매핑 해줘야함
                    status->process_running->virtual_memory[inputpageidindex]->frame_number = i;
                }
            }
        }
        else
        { // 나머지 경우
            if (status->physical_memory[i]->pid == -1)
            {
                status->process_running->virtual_memory[inputpageidindex]->frame_number = i;
                // 참조된 사이클, 새로 물리메모리에 할당되므로 참조횟수 1로 초기화
                status->process_running->virtual_memory[inputpageidindex]->refercycle = status->cycle;
                status->process_running->virtual_memory[inputpageidindex]->refercount = 1; // 참조횟수 초기화
                status->physical_memory[i] = status->process_running->virtual_memory[inputpageidindex];
                status->process_running->page_table[inputpageidindex]->frame_number = i;
                status->process_running->page_table[inputpageidindex]->R_W_bit = 'W'; // ?
                break;
            }
        }
    }
}
// page를 제거만 하는 함수(mem_allocate 함수에 사용)
void remove_page(const string algorithm, Status *status)
{
    // 알고리즘에 따라 교체되어야할 page를 찾고 뺀다.
    if (algorithm == "lru")
    {
        lru(status);
    }
    else if (algorithm == "fifo")
    {
        fifo(status);
    }
    else if (algorithm == "lfu")
    {
        lfu(status);
    }
    else if (algorithm == "mfu")
    {
        mfu(status);
    }
    else
    {
        cout << "ERROR : Invalid page swap algorithm input" << endl;
        assert(algorithm == "lru" || algorithm == "fifo" || algorithm == "lfu" || algorithm == "mfu"); // assert 실패로 프로그램 종료
    }
}

void fault(Status *status, const string algorithm, const int inputpageidindex) // 메모리 교체 알고리즘에 따라 fault 처리(메모리 교체)
{
    status->command = "fault";
    status->mode = "kernel";
    change_page(algorithm, status, inputpageidindex);
    // fault 처리 후 ready queue에 삽입
    status->process_ready.push(status->process_running);
    status->process_running = NULL;
    print_status(*status);
    status->mode = "user";
    status->command = "none";
}

// 메모리 할당 함수
void memory_allocate(Status *status, const int size, const string algorithm)
{
    // physical memory에 size만큼 페이지 할당, 만약 physical memory에 빈 자리가 없다면
    // fault 처리를 통해 알고리즘에 따라 부족한 만큼 change_page를 반복해 물리 메모리에서 할당을 제거한다.
    // 그 후 물리 메모리에 page를 할당 후 page table에 frame number를 기록한다.
    // 물리 메모리의 빈공간 갯수 세기
    int emptycount = 0;
    for (Page *page : status->physical_memory)
    {
        if (page->pid == -1)
            emptycount++;
    }
    // 할당하려는 사이즈-빈공간의 갯수 만큼 알고리즘에 따라 물리메모리에서 페이지 제거하고 테이블의 매핑 지운다.
    // 하나 빼고 하나 넣는 방식이 아니라, arg1만큼의 빈 자리를 먼저 만들어 낸 후 확보된 곳에 새로운 메모리를 할당한다.
    for (int i = 0; i < (size - emptycount); i++)
    {
        remove_page(algorithm, status);
    }

    status->process_running->alloc_id++; // allocate가 실핼 될 때 프로세스의 alloc_id 1 증가
    // virtual memory에 size만큼 페이지 할당
    for (int i = 0; i < size; i++)
    {
        Page *page = new Page; // page 갯수만큼 다른 page 포인터 생성
        page->pid = status->process_running->pid;
        page->page_number = status->process_running->alloc_count; // page_number는 지금까지 프로세스의 allocate된 횟수+1부터 시작
        page->frame_number = -1;
        page->alloc_id = status->process_running->alloc_id; // 할당할 페이지에 alloc_id 기록
        page->refercycle = status->cycle;
        page->refercount = 1; // 물리 메모리에 새로 할당하므로 참조횟수 1로 초기화
        for (int j = 0; j < status->process_running->virtual_memory.size(); j++)
        {
            if (status->process_running->virtual_memory[j]->pid == -1) // virtual memory에서 앞부분부터 빈 자리에 page 할당, vm이 가득 차는경우는 고려 X
            {
                status->process_running->virtual_memory[j] = page;                                   // 빈 자리에 페이지 할당
                status->process_running->page_table[j] = status->process_running->virtual_memory[j]; // page table에 page 할당
                // 위에서 추가한 페이지를 물리 메모리에 할당
                for (int k = 0; k < status->physical_memory.size(); k++)
                {
                    if (status->physical_memory[k]->pid == -1) // physical memory의 frame[j]가 비어있으면
                    {
                        status->process_running->virtual_memory[j]->frame_number = k;            // page에 할당된 frame number 저장
                        status->physical_memory[k] = status->process_running->virtual_memory[j]; // frame에 페이지 할당
                        // page_table에 들어가있는 page ptr는 virtual memory에 들어가있는 page ptr와 같은 page 객체 참조
                        status->process_running->page_table[j] = status->process_running->virtual_memory[j]; // frame number가 j인 page를 page table에 기록(i 번째 page의 frame number 기록)
                        break;
                    }
                    else // physical memory의 frame이 차 있으면 건너뛴다.
                    {
                        continue;
                    }
                }
                break;
            }
        }
        status->process_running->alloc_count++; // 현재 allocate한 page id의 처리가 끝난 후  다음 page id 할당 위해 allocate 명령 횟수 세기
    }
}

void memory_release(Status *status, const int inputallocid)
// if 삭제하려는게 R 이고 부모 프로세스의 명령이라면, R 이더라도 물리메모리에서 날려야한다. 물리 메모리에 저장되어 있는 R 권한 페이지는 공유 중이지만 무조건 부모의 페이지이기 떄문이다.
// 부모 프로세스는 공유중인 페이지를 자식에게 복사하면서 W 권한으로 바꿔주고 지정된 페이지를 삭제한다.
// 자식 프로세스는 R 권한이라면 물리 메모리에서는 삭제하지 않고 공유중인 자원의 권한을 전부 W로 바꾼다.
{
    // physical memory에서 alloc_id가 inputallocid이고 권한이 W인 페이지를 찾아서 free
    for (int i = 0; i < status->physical_memory.size(); i++)
    { // 물리 메모리에 할당된 page의 allocate_id와 pid가 현재 프로세스의 것과 같으면 free
        // 자식 프로세스의 page는 쓰기나 읽기 명령이 들어왔을 때 fault 명령 발생 후 frame에 할당된다.
        if (status->physical_memory[i]->alloc_id == inputallocid && status->physical_memory[i]->pid == status->process_running->pid && status->physical_memory[i]->R_W_bit == 'W')
        {
            status->physical_memory[i] = status->emptypage; // free
        }
        // 현재 프로세스가 init(부모 프로세스)이고  allocate_id와 pid가 init 의 것과 같고 R_W_bit가 R이면 물리 메모리에서 삭제(부모 프로세스의 페이지 삭제 후 자식 프로세스로 copy)
        else if (status->physical_memory[i]->alloc_id == inputallocid && status->process_running->pid == 1 && status->physical_memory[i]->pid == 1 && status->physical_memory[i]->R_W_bit == 'R')
        {
            status->physical_memory[i] = status->emptypage; // R 권한이여도 물리 메모리에서 삭제
        }
        else
        { // 자식 프로세스의 R인 페이지는 물리 메모리에서 삭제하지 않고 아래서 권한을 변경후 가상 메모리에서만 삭제한다.
            continue;
        }
    }

    // virtual memory에서 alloc_id가 inputallocid인 page를 찾아서 free
    for (int i = 0; i < status->process_running->virtual_memory.size(); i++)
    {
        if (status->process_running->virtual_memory[i]->alloc_id == inputallocid)
        {                                                                   // 부모 프로세스는 공유중인 페이지를 자식에게 복사하면서 W 권한으로 바꿔주고 지정된 페이지를 삭제한다.
            if (status->process_running->virtual_memory[i]->R_W_bit == 'R') // 해제할 페이지가 자식 프로세스에서 공유중인 자원이라면
            {
                for (int j = 0; j < status->process_ready.size(); j++)
                {
                    if (status->process_ready.front()->virtual_memory[i]->alloc_id == inputallocid && status->process_ready.front()->virtual_memory[i]->R_W_bit == 'R')
                    {
                        status->process_ready.front()->virtual_memory[i]->R_W_bit = 'W'; // 자식 프로세스의 page의 R_W_bit = W 변경
                        status->process_ready.push(status->process_ready.front());
                        status->process_ready.pop();
                    }
                    else
                    {
                        status->process_ready.push(status->process_ready.front());
                        status->process_ready.pop();
                    }
                }
                for (int j = 0; j < status->process_waiting.size(); j++)
                {
                    if (status->process_waiting.front()->virtual_memory[i]->alloc_id == inputallocid && status->process_waiting.front()->virtual_memory[i]->R_W_bit == 'R')
                    {
                        status->process_waiting.front()->virtual_memory[i]->R_W_bit = 'W'; // 자식 프로세스의 page의 R_W_bit = W 변경
                        status->process_waiting.push(status->process_waiting.front());
                        status->process_waiting.pop();
                    }
                    else
                    {
                        status->process_waiting.push(status->process_waiting.front());
                        status->process_waiting.pop();
                    }
                }
                // 권한을 바꾸고 자식 프로세스로 copy 한 후 프로세스의 페이지 삭제
                status->process_running->virtual_memory[i] = status->process_running->emtpypage;     // free
                status->process_running->page_table[i] = status->process_running->virtual_memory[i]; // page table에서도 free
            }
            else // R_W_bit == 'W'
            {
                status->process_running->virtual_memory[i] = status->process_running->emtpypage;     // 부모가 W면 그냥 할당 해제
                status->process_running->page_table[i] = status->process_running->virtual_memory[i]; // page table에서도 free
            }
        }
    }
}

void memory_read(Status *status, const int inputpageid, const string algorithm)
{
    int inputpageidindex;                                                    // 읽을 page id의 index
    for (int i = 0; i < status->process_running->virtual_memory.size(); i++) // 읽을 page id 찾기
    {
        if (status->process_running->virtual_memory[i]->page_number == inputpageid)
        {
            inputpageidindex = i;
            break;
        }
    }
    // 자식 프로세스에서 일단 페이지가 할당되어 있고 그 페이지의 권한이 R 인 상태라면, page table에 매핑이 풀려 있어도(frame_number = -1 이어도)
    // 자식 프로세스가 부모 프로세스의 페이지를 공유받고 있는 상태이므로, read 명령에서는 새로운 페이지를 가상메모리에 할당하는 것이 아니라
    // 공유받는 부모의 페이지를 물리메모리에 넣고 부모와 자식의 page table에 모두 매핑해주면 된다.(계속 R 권한)
    // 현재 프로세스가 자식 프로세스이고 읽으려는 페이지가 물리 메모리에 존재하지 않으며
    // 자식 프로세스의 페이지가 부모 프로세스의 페이지를 공유받고 있는 상태이면
    if (status->process_running->page_table[inputpageidindex]->frame_number == -1 && status->process_running->pid != 1 && status->process_running->page_table[inputpageidindex]->R_W_bit == 'R')
    {
        print_status(*status);                      // 폴트 발생 전 상태 출력
        fault(status, algorithm, inputpageidindex); // 폴트 발생, 보통 fault 처리와는 다르게 waiting에 있는 부모
    }
    // R인 페이지가 아닌 W, 즉 공유되지 않는 페이지거나 자식 프로세스가 아닌 경우
    else if (status->process_running->page_table[inputpageidindex]->frame_number == -1                                                                                                                // frame이 할당되지 않은 page거나
             || status->physical_memory[status->process_running->page_table[inputpageidindex]->frame_number]->alloc_id != status->process_running->virtual_memory[inputpageidindex]->alloc_id         // alloc_id가 다르거나
             || status->physical_memory[status->process_running->page_table[inputpageidindex]->frame_number]->frame_number != status->process_running->virtual_memory[inputpageidindex]->frame_number // frame_number가 다르거나
             || status->physical_memory[status->process_running->page_table[inputpageidindex]->frame_number]->R_W_bit != status->process_running->virtual_memory[inputpageidindex]->R_W_bit)          // R_W_bit가 다르면, 즉 아예 다른 page가 물리메모리에 있으면
    {
        print_status(*status);                      // 폴트 발생 전 상태 출력
        fault(status, algorithm, inputpageidindex); // 페이지 폴트 발생, inputpageidindex에 해당하는 page를 물리 메모리에 할당, 자리 없으면 교체도 함
        // 할당된 page의 frame_number를 page table에 저장
    }
    else
    { // pid만 다르거나(즉 부모-자식 관계의 page거나) 아예 같은 page라면
        // 물리 메모리의 한 frame을 read 한 시점의 cycle 저장
        status->physical_memory[status->process_running->page_table[inputpageidindex]->frame_number]->refercycle = status->cycle;
        status->physical_memory[status->process_running->page_table[inputpageidindex]->frame_number]->refercount++; // 참조 횟수 증가
        print_status(*status);                                                                                      // 페이지 읽고 cycle 상태 출력 후 다음 유저 명령 실행
    }
}
void memory_write(Status *status, int inputpageid, const string algorithm)
{
    int inputpageidindex;                                                    // 읽을 page id의 index
    for (int i = 0; i < status->process_running->virtual_memory.size(); i++) // 읽을 page id 찾기
    {
        if (status->process_running->virtual_memory[i]->page_number == inputpageid)
        {
            inputpageidindex = i;
            break;
        }
    }
    if (status->process_running->page_table[inputpageidindex]->frame_number == -1                                                                                                                // frame이 할당되지 않은 page거나
        || status->physical_memory[status->process_running->page_table[inputpageidindex]->frame_number]->alloc_id != status->process_running->virtual_memory[inputpageidindex]->alloc_id         // alloc_id가 다르거나
        || status->physical_memory[status->process_running->page_table[inputpageidindex]->frame_number]->frame_number != status->process_running->virtual_memory[inputpageidindex]->frame_number // frame_number가 다르거나
        || status->physical_memory[status->process_running->page_table[inputpageidindex]->frame_number]->R_W_bit != status->process_running->virtual_memory[inputpageidindex]->R_W_bit)          // R_W_bit가 다르면, 즉 아예 다른 page가 물리메모리에 있으면
    {
        print_status(*status);                      // 폴트 발생 전 상태 출력
        fault(status, algorithm, inputpageidindex); // 페이지 fault: page 교체
        // 빈 페이지(프레임)가 있으면, 가장 왼쪽 빈 페이지에 할당하고, 없으면 알고리즘 써서 빼내고 할당. 명세서참고
    }
    else if (status->process_running->page_table[inputpageidindex]->R_W_bit == 'R') // 페이지가 물리 메모리에 있고 읽기 전용 page라면
    {
        print_status(*status); // protection 폴트 발생 전 상태 출력
        status->command = "fault";
        status->mode = "kernel";

        int a = status->process_running->virtual_memory[inputpageidindex]->alloc_id;
        if (status->process_running->pid == 1) // 부모 프로세스라면
        {                                      // 부모와 자식 process의 page의 R_W_bit를 W로 바꿔준다.
            status->process_running->virtual_memory[inputpageidindex]->R_W_bit = 'W';
            for (int j = 0; j < status->process_ready.size(); j++)
            {
                if (status->process_ready.front()->virtual_memory[inputpageidindex]->alloc_id == a && status->process_ready.front()->virtual_memory[inputpageidindex]->R_W_bit == 'R')
                {
                    status->process_ready.front()->virtual_memory[inputpageidindex]->R_W_bit = 'W'; // 자식 프로세스의 page의 R_W_bit = W 변경
                    status->process_ready.push(status->process_ready.front());
                    status->process_ready.pop();
                }
                else
                {
                    status->process_ready.push(status->process_ready.front());
                    status->process_ready.pop();
                }
            }
            for (int j = 0; j < status->process_waiting.size(); j++)
            {
                if (status->process_waiting.front()->virtual_memory[inputpageidindex]->alloc_id == a && status->process_waiting.front()->virtual_memory[inputpageidindex]->R_W_bit == 'R')
                {
                    status->process_waiting.front()->virtual_memory[inputpageidindex]->R_W_bit = 'W'; // 자식 프로세스의 page의 R_W_bit = W 변경
                    status->process_waiting.push(status->process_waiting.front());
                    status->process_waiting.pop();
                }
                else
                {
                    status->process_waiting.push(status->process_waiting.front());
                    status->process_waiting.pop();
                }
            }
        }
        else // 자식 프로세스라면
        {
            status->process_running->virtual_memory[inputpageidindex]->R_W_bit = 'W'; // 현재 프로세스 page
            for (int j = 0; j < status->process_ready.size(); j++)
            {
                if (status->process_ready.front()->virtual_memory[inputpageidindex]->alloc_id == a && status->process_ready.front()->virtual_memory[inputpageidindex]->R_W_bit == 'R')
                {
                    status->process_ready.front()->virtual_memory[inputpageidindex]->R_W_bit = 'W'; // 자식 프로세스의 page의 R_W_bit = W 변경
                    status->process_ready.push(status->process_ready.front());
                    status->process_ready.pop();
                }
                else
                {
                    status->process_ready.push(status->process_ready.front());
                    status->process_ready.pop();
                }
            }
            for (int j = 0; j < status->process_waiting.size(); j++)
            {
                if (status->process_waiting.front()->virtual_memory[inputpageidindex]->alloc_id == a && status->process_waiting.front()->virtual_memory[inputpageidindex]->R_W_bit == 'R')
                {
                    status->process_waiting.front()->virtual_memory[inputpageidindex]->R_W_bit = 'W'; // 자식 프로세스의 page의 R_W_bit = W 변경
                    status->process_waiting.push(status->process_waiting.front());
                    status->process_waiting.pop();
                }
                else
                {
                    status->process_waiting.push(status->process_waiting.front());
                    status->process_waiting.pop();
                }
            }
            // physical_memory에 page 할당
            fault(status, algorithm, inputpageidindex); // 페이지 fault: page 교체
        }
    }
    else
    { // page가 물리메모리에 있고 R_W_bit가 W라면
        // 물리 메모리의 한 frame을 read 한 시점의 cycle 저장
        status->physical_memory[status->process_running->page_table[inputpageidindex]->frame_number]->refercycle = status->cycle;
        status->physical_memory[status->process_running->page_table[inputpageidindex]->frame_number]->refercount++; // 참조 횟수 증가
        print_status(*status);                                                                                      // 페이지 쓰고 cycle 상태 출력 후 다음 유저 명령 실행
    }
}
/*
cycle 을 클래스로 구현하거나(사이클 수치가 있는)
맨 위에 run, sleep, wait 일때 명령을 구현하고
매 cycle을 1씩 늘리고
맨 아래엔 status 출력 할때마다 조건문을 걸어서 (사이클이 현재+얼마일 떼
큐를 넣는다거나 하는 함수를 만들어서) if cycle == (명령했을때 사이클 + 수치) ~ */

// 사이클 단위로 구현되는 커널 시뮬레이터

int main(int argc, char *argv[])
{
    // 프로세스를 얼마만큼 run 할지와, 그 프로세스의 pid를 저장하는 벡터
    vector<pair<int, int>> run_list;

    // 프로세스를 얼마만큼 sleep 할지와, 그 프로세스의 pid를 저장하는 벡터
    vector<pair<int, int>> sleep_list;

    // argc : 실행하는 프로그램을 포함한 인자 개수, ./project2 ../input ->argc = 2
    // argv : 배열에 저장. 첫번째 인자는 프로그램 이름, 그 다음은 입력받은 경로
    string datapath = argv[1];
    // argv[2] : 페이지 교체 알고리즘이 주어진다: lru, fifo, lfu, mfu
    string algorithm = argv[2];
    // result 파일을 저장할 경로
    string currentpath = std::filesystem::current_path().string();

    Status status;
    status.cycle = 0;
    status.mode = "user";
    status.command = "none";
    status.process_running = NULL;
    status.process_new = NULL;
    status.process_terminated = NULL;

    vector<string> initline = read_file("init", datapath, currentpath);
    // init 프로세스 생성
    Process *p1 = new Process();
    p1->name = "init";
    p1->pid = 1;
    p1->ppid = 0;
    p1->waiting_type = 'S';
    p1->lines = initline;

    status.mode = "kernel";
    status.command = "boot";
    status.process_new = p1;

    // 출력 스트림을 result 파일로 변경
    open_stream_to_file(status, "result");

    print_status(status);
    ////////////////////////////////////////////cycle 1
    while (1)
    {
        // 1. (*sleep 시간 갱신), wait, run cycle 처리
        // sleep, wait 상태의 프로세스 중 ready 큐로 넣을 프로세스가 있는지 확인
        // run cycle이 0이 되면 run 명령 상태 해제
        sleep_list_update(status, sleep_list);
        run_list_update(status, run_list);
        // 2. 프로세스 상태 갱신(new, terminated)
        // process_new가 있으면 ready 큐에 넣음
        if (status.process_new != NULL)
        {
            status.process_ready.push(status.process_new);
            status.process_new = NULL;
        }
        // process_terminated가 있으면 NULL로 만듦(삭제)
        if (status.process_terminated != NULL)
        {
            status.process_terminated = NULL;
        }
        // 3. Ready queue 갱신같은 경우, 내 cycle 알고리즘에선 sleep 시간이 0이 될때
        // ready 큐에 갱신하는게 아니라 (sleep 시간 + 명령 실행된 cycle) = 현재 cycle 일때
        // sleep_list_update 함수에서 갱신하였다. 즉 1번과정이 3번에 통합되어있다고 볼 수 있다.
        //
        // run 명령 상태일 때 실행
        if (status.command.substr(0, 3) == "run")
        {
            print_status(status);
        }
        // 4. 커널 모드 또는 유저 모드의 명령 실행
        // 먼저 running 상태인 프로세스가 있는지 확인, 없으면 ready 큐에서 꺼내서 running 상태로 만듦
        // running 상태인 프로세스가 있으면 명령을 확인하고 실행
        // 명령이 fork_and_exec이면 새로운 프로세스를 만들고 ready 큐에 넣음
        if (status.process_running != NULL && status.command.substr(0, 3) != "run")
        {
            // command : memoory_allocate 이면
            if (read_word(status.process_running)[0] == "memory_allocate")
            {
                status.mode = "user";
                status.command = status.process_running->lines.front();
                int size = stoi(read_word(status.process_running)[1]);                      // 할당할 memory 크기
                status.process_running->lines.erase(status.process_running->lines.begin()); // 읽은 명령 삭제
                print_status(status);
                // 사이클 1회 완료(memory_allocate 명령어 실행)

                // 메모리 할당
                status.mode = "kernel";
                status.command = "system call";
                memory_allocate(&status, size, algorithm);
                // 커널 모드 처리 후 ready queue에 삽입
                status.process_ready.push(status.process_running);
                status.process_running = NULL;

                print_status(status);
            }
            // command : memory_release 이면
            else if (read_word(status.process_running)[0] == "memory_release")
            {
                status.mode = "user";
                status.command = status.process_running->lines.front();
                int inputallocid = stoi(read_word(status.process_running)[1]);
                status.process_running->lines.erase(status.process_running->lines.begin()); // 읽은 명령 삭제
                print_status(status);
                // 사이클 1회 완료(memory_release 명령어 실행)

                // 메모리 해제
                status.mode = "kernel";
                status.command = "system call";
                memory_release(&status, inputallocid);
                // 커널 모드 처리 후 ready queue에 삽입
                status.process_ready.push(status.process_running);
                status.process_running = NULL;

                print_status(status);
            }
            // command : fork and exec 이면
            else if (read_word(status.process_running)[0] == "fork_and_exec")
            {
                status.mode = "user";
                status.command = status.process_running->lines.front();

                print_status(status);
                // 사이클 1회 완료(fork_and_exec 명령어 실행)

                // program1 파일에서 명령어 읽어서 새 프로세스에 넣기
                status.mode = "kernel";
                status.command = "system call";
                status.process_new = make_process(status.process_running, datapath, currentpath); // fork_and_exec program1
                status.process_ready.push(status.process_running);
                status.process_running = NULL;

                print_status(status);
            }
            // command : sleep 이면
            else if (read_word(status.process_running)[0] == "sleep")
            {
                status.command = status.process_running->lines.front();
                sleep_list.push_back(make_pair(status.process_running->pid, status.cycle + stoi(read_word(status.process_running)[1])));
                // 읽은 명령 삭제
                status.process_running->lines.erase(status.process_running->lines.begin());
                // sleep_list에 pid와 얼마만큼 sleep 할지 저장
                sleep_list_update(status, sleep_list);

                print_status(status);
                // 사이클 1회 완료(sleep 명령어 실행)

                status.mode = "kernel";
                status.command = "system call";
                status.process_running->waiting_type = 'S';
                status.process_waiting.push(status.process_running);
                status.process_running = NULL;
                // sleeplist update 한번 해주기(sleep 1인 경우를 위해)
                sleep_list_update(status, sleep_list);

                print_status(status);
                status.command = "none"; // system call-sleep 끝
            }
            // command : wait 면
            else if (read_word(status.process_running)[0] == "wait")
            {
                status.command = status.process_running->lines.front();
                // 읽은 명령 삭제
                status.process_running->lines.erase(status.process_running->lines.begin());
                sleep_list_update(status, sleep_list);

                print_status(status);
                // 사이클 1회 완료(wait 명령어 실행)

                status.mode = "kernel";
                status.command = "system call";
                status.process_running->waiting_type = 'W';
                status.process_waiting.push(status.process_running);
                status.process_running = NULL;
                // check 여기서도 한번 하기
                sleep_list_update(status, sleep_list);

                print_status(status);
                status.command = "none"; // system call-wait 끝
            }
            // command : run 이면
            else if (read_word(status.process_running)[0] == "run")
            {
                status.mode = "user";
                status.command = status.process_running->lines.front();
                run_list.push_back(make_pair(status.process_running->pid, status.cycle + stoi(read_word(status.process_running)[1])));
                // 읽은 명령 삭제
                status.process_running->lines.erase(status.process_running->lines.begin());

                print_status(status);
                // 사이클 1회 완료(run 명령어 실행)
                // kernel 모드로 들어가지 않으므로 사이클을 소비하지 않음
            }
            else if (read_word(status.process_running)[0] == "memory_read")
            {
                status.mode = "user";
                status.command = status.process_running->lines.front();
                int inputpageid = stoi(read_word(status.process_running)[1]);
                status.process_running->lines.erase(status.process_running->lines.begin()); // 읽은 명령 삭제
                memory_read(&status, inputpageid, algorithm);
            }
            else if (read_word(status.process_running)[0] == "memory_write")
            {
                status.mode = "user";
                status.command = status.process_running->lines.front();
                int inputpageid = stoi(read_word(status.process_running)[1]);
                status.process_running->lines.erase(status.process_running->lines.begin()); // 읽은 명령 삭제
                memory_write(&status, inputpageid, algorithm);
            }
            // command : exit 면
            else if (read_word(status.process_running)[0] == "exit")
            {
                status.command = status.process_running->lines.front();
                // 읽은 명령 삭제
                status.process_running->lines.erase(status.process_running->lines.begin());
                sleep_list_update(status, sleep_list);

                print_status(status);
                // 사이클 1회 완료(terminate 명령어 실행)

                status.mode = "kernel";
                status.command = "system call";
                // memory release 과정 : 현재 프로세스의 pid와 같고 권한이 W인 page를 모두 release
                for (int i = 0; i < status.physical_memory.size(); i++)
                {
                    if (status.physical_memory[i]->pid == status.process_running->pid && status.physical_memory[i]->R_W_bit == 'W')
                    {
                        status.physical_memory[i] = status.emptypage; // 빈 frame으로 바꿈
                    }
                }
                status.process_terminated = status.process_running;
                status.process_running = NULL;
                sleep_list_update(status, sleep_list);

                // 자식 프로세스가 terminated 로 이동한 후, 현재 프로세스가 부모 뿐이면 부모의 page R_W_bit을 W로 바꿈(공유되는 자식이 없음)
                vector<Process *> temp; // running 프로세스와 ready, waiting queue에 있는 프로세스들을 임시로 저장할 벡터
                if (status.process_running != NULL)
                {
                    temp.push_back(status.process_running);
                }
                for (int i = 0; i < status.process_ready.size(); i++)
                {
                    temp.push_back(status.process_ready.front());
                    status.process_ready.push(status.process_ready.front());
                    status.process_ready.pop();
                }
                for (int i = 0; i < status.process_waiting.size(); i++)
                {
                    temp.push_back(status.process_waiting.front());
                    status.process_waiting.push(status.process_waiting.front());
                    status.process_waiting.pop();
                }
                if (temp.size() == 1)
                {                                              // 현재 프로세스가 부모 뿐이라면(공유중인 자식이 없다면)
                    for (Page *page : temp[0]->virtual_memory) // 부모 프로세스의 page에 접근, 모두 W로 변경
                    {
                        if (page->R_W_bit == 'R')
                        {
                            page->R_W_bit = 'W';
                        }
                    }
                }

                print_status(status);
                status.command = "none"; // system call-terminate 끝
            }
            else
            {
                if (status.process_ready.size() != 0)
                {
                    sleep_list_update(status, sleep_list);

                    schedule(&status);
                }
                else
                {
                }
            }
        }
        else if (status.process_running == NULL) // run 상태일때는 이 스케줄링 구문을 지나가고, run 상태가 아닐때만 스케줄링을 해줘야함
        {                                        // run 상태일떄는 스케줄링도 하면 안되기 떄문에
            // 해당 시점에 레디 큐가 비어있지 않으면 다음 프로세스 선택(스케줄링)
            if (status.process_ready.size() != 0)
            {
                sleep_list_update(status, sleep_list);

                schedule(&status);
            }
            // 해당 시점에 레디 큐가 빈 경우, 그리고 waiting중인 프로세스가 있으면 idle 동작 진행
            else if (status.process_waiting.size() != 0)
            {
                sleep_list_update(status, sleep_list);

                idle(&status);
            }
        }
        // debug용 assert
        if (status.process_running != NULL)
        {
            assert(read_word(status.process_running)[0] == "memory_allocate" || read_word(status.process_running)[0] == "fork_and_exec" ||
                   read_word(status.process_running)[0] == "sleep" || read_word(status.process_running)[0] == "wait" || read_word(status.process_running)[0] == "exit" || read_word(status.process_running)[0] == "memory_read" || read_word(status.process_running)[0] == "memory_write" || read_word(status.process_running)[0] == "run" || read_word(status.process_running)[0] == "memory_release");
        }
        // 실행중인 프로세스도 없고 ready 큐에도 프로세스가 없고 waiting에도 없으면 프로그램 종료
        if (status.process_ready.size() == 0 && status.process_running == NULL && status.process_waiting.size() == 0 && status.process_new == NULL)
        {
            break;
        }
    }
    freopen("/dev/tty", "w", stdout); // 스트림 콘솔로 돌려놓기
    // cout << "프로그램 종료" << endl;
    return 0;
}