#include <mutex>
#include <vector>
#include <tuple>
#include <memory>
#include <queue>
#include <semaphore>
#include <iostream>
#include <chrono>
#include <thread>

namespace protocol
{

struct NetPacketHeader
{
    uint32_t id;
};

struct Packet
{
    NetPacketHeader header;
    std::vector<uint8_t> data;
};

const uint32_t WorkerFunctionInvokeId = 1;
const uint32_t WorkerStopId = 2;

struct WorkerFunctionInvoke
{
    static constexpr const uint32_t id = WorkerFunctionInvokeId;
    NetPacketHeader header;

    intptr_t function;

    WorkerFunctionInvoke() : header { id } {}

    Packet serialize()
    {
        Packet p;
        p.header = header;
        p.data.resize(sizeof(function));
        memcpy(p.data.data(), &function, sizeof(function));
        return p;
    }

    bool deserialize(Packet p)
    {
        header = std::move(p.header);
        if (p.data.size() < sizeof(function));
        else
        {
            memcpy(&function, p.data.data(), sizeof(function));
            return true;
        }
        return false;
    }
};

}

struct PacketBinder
{
    template <typename Object, typename Packet>
    void bind(Object* _this, void(Object::* memfn)(Packet))
    {
        handlers[Packet::id] = [_this, memfn](protocol::Packet packet)
        {
            Packet body;
            if (body.deserialize(packet))
            {
                (_this->*memfn)(body);
            }
        };
    }

    bool handlePacket(protocol::Packet packet)
    {
        if (auto handler = handlers.find(packet.header.id); handler != handlers.end())
        {
            (handler->second)(std::move(packet));
            return true;
        }
        return false;
    }

private:
    std::map<uint32_t, std::function<void(const protocol::Packet&)>> handlers;
};

class BasicWorker
{
public:
    BasicWorker(int tls) : tls(tls) {}

    virtual ~BasicWorker() {}

    template<typename PACKET>
    bool pushPacket(PACKET packet)
    {
        return pushPacket(packet.serialize());
    }

    template<>
    bool pushPacket(protocol::Packet packet)
    {
        std::scoped_lock lock(mutex);
        queue.push(std::move(packet));
        sem.release();
        return true;
    }

    void run()
    {
        while (bRun)
        {
            sem.acquire();
            while (!queue.empty())
            {
                std::scoped_lock lock(mutex);
                auto packet = queue.front();
                binder.handlePacket(packet);
                queue.pop();
            }
        }
    }

    void stop()
    {
        bRun = false;
        sem.release();
    }

    void test(std::string str)
    {
        std::cout << "test tls " << tls << " " << str << std::endl;
    }

    std::optional<std::thread> thread;

protected:
    PacketBinder binder;

private:
    int tls;
    std::mutex mutex;
    std::queue<protocol::Packet> queue;
    std::binary_semaphore sem { 0 };
    bool bRun = true;
};

void runWorker(BasicWorker* worker)
{
    worker->run();
}

class Application
{
public:
    Application(int workersCount);

    ~Application();

    void runWorkers();

    template <typename Worker>
    struct IHandler
    {
        virtual void operator()(Worker* worker) = 0;
        virtual ~IHandler() {}
    };

    template <typename Worker, typename...Args>
    bool executeInWorker(uint32_t tls, void(Worker::* memfun)(Args...), std::tuple<std::decay_t<Args>...> args)
    {
        auto adapter = new MemFnAdaptHandler(memfun, std::move(args));
        auto intr = protocol::WorkerFunctionInvoke {};
        intr.function = reinterpret_cast<intptr_t>(adapter);
        if (tls > collectionOfWorkers.size() || !collectionOfWorkers[tls] || !collectionOfWorkers[tls]->pushPacket(intr))
        {
            delete adapter;
            return false;
        }
        return true;
    }

private:
    template <typename Worker, typename...Args>
    struct MemFnAdaptHandler : IHandler<Worker>
    {
        MemFnAdaptHandler(void(Worker::* memfun)(Args...), std::tuple<std::decay_t<Args>...> args)
            : memfun(memfun)
            , args(std::move(args))
        {}
        virtual void operator()(Worker* worker) override
        {
            std::apply(memfun, std::tuple_cat(std::make_tuple(worker), std::move(args)));
        }
    private:
        void(Worker::* memfun)(Args...);
        std::tuple<std::decay_t<Args>...> args;
    };

    std::vector<std::shared_ptr<BasicWorker>> collectionOfWorkers;
};

class Worker : public BasicWorker
{
public:
    Worker(int tls) : BasicWorker(tls)
    {
        binder.bind(this, static_cast<void(Worker::*)(protocol::WorkerFunctionInvoke)>(&Worker::handle));
    }

private:
    void handle(protocol::WorkerFunctionInvoke packet)
    {
        if (packet.function)
        {
            auto& adapter = *reinterpret_cast<Application::IHandler<Worker>*>(packet.function);
            adapter(static_cast<Worker*>(this));
            delete& adapter;
        }
    }
};

Application::Application(int workersCount)
{
    for (auto i = 0; i < workersCount; i++)
    {
        collectionOfWorkers.push_back(std::make_shared<Worker>(i));
    }
}

Application::~Application()
{
    for (auto& worker : collectionOfWorkers)
    {
        if (worker)worker->stop();
    }
    for (auto& worker : collectionOfWorkers)
    {
        if (worker)worker->thread->join();
    }
}

void Application::runWorkers()
{
    for (auto& worker : collectionOfWorkers)
    {
        worker->thread = std::thread(runWorker, worker.get());
    }
}

int main()
{
    Application app(3);
    app.runWorkers();
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(2000ms);
    app.executeInWorker(1, &Worker::test, { "abc" });
    app.executeInWorker(2, &Worker::test, { "def" });
}
