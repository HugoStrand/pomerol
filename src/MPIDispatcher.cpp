#include "MPIDispatcher.h"

namespace Pomerol { 

namespace pMPI {

//
// Worker
//
MPIWorker::MPIWorker(const boost::mpi::communicator &comm, WorkerId boss):
        Comm(comm),
        id(Comm.rank()),
        boss(boss),
        WorkReq(Comm.irecv(boss, int(WorkerTag::Work), current_job)),
        FinishReq(Comm.irecv(boss, int(WorkerTag::Finish))),
        Status(WorkerTag::Pending)
    {};



bool MPIWorker::is_finished()
{
return (Status == WorkerTag::Finish);
}

bool MPIWorker::is_working()
{
return (Status == WorkerTag::Work);
}

void MPIWorker::receive_order() 
{
    if (Status == WorkerTag::Pending && WorkReq.test()) { Status = WorkerTag::Work; return; }; 
    if (Status == WorkerTag::Pending && FinishReq.test()) { Status = WorkerTag::Finish; WorkReq.cancel(); return; };
}

void MPIWorker::report_job_done()
{ 
    auto send_req = Comm.isend(boss,int(WorkerTag::Pending)); 
    DEBUG(id << "->" << boss << " tag: pending");
    Status = WorkerTag::Pending; 
    WorkReq = Comm.irecv(boss, int(WorkerTag::Work), current_job);
};
//
// Master
//

MPIMaster::MPIMaster(const boost::mpi::communicator &comm, std::vector<WorkerId> worker_pool, std::vector<JobId> task_numbers ):
    Comm(comm), Ntasks(task_numbers.size()),
    Nprocs(worker_pool.size()), id(Comm.rank()),
    task_numbers(task_numbers), worker_pool(worker_pool), 
    wait_statuses(Nprocs),
    workers_finish(Nprocs,false)
{
    for (int i=Ntasks-1; i>=0; i--) { JobStack.push(task_numbers[i]); };
    for (int p=Nprocs-1; p>=0; p--) { 
        WorkerIndices[worker_pool[p]] = p; 
        WorkerStack.push(worker_pool[p]); 
        };
};

inline std::vector<WorkerId> _autorange_workers(const boost::mpi::communicator &comm, bool include_boss)
{
    std::vector<WorkerId> out;
    size_t Nprocs(comm.size()-!include_boss);
    if (!Nprocs) throw (std::logic_error("No workers to evaluate"));
    for (size_t p=0; p<comm.size(); p++) { 
        if (include_boss || comm.rank() != p) {
            out.push_back(p); 
            };
        };
    return out;
}

inline std::vector<JobId> _autorange_tasks(size_t ntasks)
{
    std::vector<JobId> out(ntasks);
    for (size_t i=0; i<ntasks; i++) { 
        out[i] = i; 
    };
    return out;
}

MPIMaster::MPIMaster(const boost::mpi::communicator &comm, size_t ntasks, bool include_boss):
    MPIMaster(comm,_autorange_workers(comm,include_boss), _autorange_tasks(ntasks))
{};

MPIMaster::MPIMaster(const boost::mpi::communicator &comm, std::vector<JobId> task_numbers, bool include_boss ):
    MPIMaster(comm,_autorange_workers(comm,include_boss), task_numbers)
{};



void MPIMaster::order_worker(WorkerId worker, JobId job)
{
    auto send_req = Comm.isend(worker,int(WorkerTag::Work),job);
    DEBUG(id << "->" << worker << " tag: work");
    send_req.wait();
    DispatchMap[job]=worker;
    wait_statuses[WorkerIndices[worker]] = Comm.irecv(worker,int(WorkerTag::Pending));
};

void MPIMaster::order()
{
    while (!WorkerStack.empty() && !JobStack.empty()) { 
        auto worker = WorkerStack.top(); 
        auto job = JobStack.top(); 
        order_worker(worker,job); 
        WorkerStack.pop(); 
        JobStack.pop(); 
    }; 
};

void MPIMaster::check_workers()
{
    if (!JobStack.empty()) {
        for (size_t i=0; i<Nprocs && !JobStack.empty(); i++) {
            if (wait_statuses[i].test()) { 
                WorkerStack.push(worker_pool[i]);
                }; 
        };
    }
    else {
        for (size_t i=0; i<Nprocs; i++) {
            if (!workers_finish[i]) { 
                DEBUG(id << "->" << worker_pool[i] << " tag: finish");
                Comm.isend(worker_pool[i],int(WorkerTag::Finish));
                workers_finish[i] = true; // to prevent double sending of Finish command that could overlap with other communication
            };
        };
    };
}

} // end of namespace MPI
} // end of namespace Pomerol

