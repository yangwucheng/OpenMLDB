//
// name_server_create_remote_test.cc
// Copyright (C) 2020 4paradigm.com
// Author wangbao 
// Date 2020-01-13
//

#include "gtest/gtest.h"
#include "logging.h"
#include "timer.h"
#include <gflags/gflags.h>
#include <sched.h>
#include <unistd.h>
#include "tablet/tablet_impl.h"
#include "proto/tablet.pb.h"
#include "proto/name_server.pb.h"
#include "name_server_impl.h"
#include "rpc/rpc_client.h"
#include <brpc/server.h>
#include "base/file_util.h"
#include "client/ns_client.h"

DECLARE_string(endpoint);
DECLARE_string(db_root_path);
DECLARE_string(zk_cluster);
DECLARE_string(zk_root_path);
DECLARE_int32(zk_session_timeout);
DECLARE_int32(request_timeout_ms);
DECLARE_int32(zk_keep_alive_check_interval);
DECLARE_int32(make_snapshot_threshold_offset);
DECLARE_uint32(name_server_task_max_concurrency);
DECLARE_bool(auto_failover);

using ::rtidb::zk::ZkClient;


namespace rtidb {
namespace nameserver {

inline std::string GenRand() {
    return std::to_string(rand() % 10000000 + 1);
}

class MockClosure : public ::google::protobuf::Closure {

public:
    MockClosure() {}
    ~MockClosure() {}
    void Run() {}

};
class NameServerImplRemoteTest : public ::testing::Test {

public:
    NameServerImplRemoteTest() {}
    ~NameServerImplRemoteTest() {}
    void Start(NameServerImpl* nameserver) {
        nameserver->running_ = true;
    }
    std::vector<std::list<std::shared_ptr<OPData>>>& GetTaskVec(NameServerImpl* nameserver) {
        return nameserver->task_vec_;
    }
    std::map<std::string, std::shared_ptr<::rtidb::nameserver::TableInfo>>& GetTableInfo(NameServerImpl* nameserver) {
        return nameserver->table_info_;
    }
    ZoneInfo& GetZoneInfo(NameServerImpl* nameserver) {
        return nameserver->zone_info_;
    }
};

TEST_F(NameServerImplRemoteTest, CreateAndDropTableRemote) {
    // local ns and tablet
    //ns
    FLAGS_zk_cluster="127.0.0.1:6181";
    FLAGS_zk_root_path="/rtidb3" + GenRand();
    FLAGS_endpoint = "127.0.0.1:9631";
    FLAGS_db_root_path = "/tmp/" + ::rtidb::nameserver::GenRand();

    NameServerImpl* nameserver_1 = new NameServerImpl();
    bool ok = nameserver_1->Init();
    ASSERT_TRUE(ok);
    sleep(4);
    brpc::ServerOptions options;
    brpc::Server server;
    if (server.AddService(nameserver_1, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        PDLOG(WARNING, "Fail to add service");
        exit(1);
    }
    if (server.Start(FLAGS_endpoint.c_str(), &options) != 0) {
        PDLOG(WARNING, "Fail to start server");
        exit(1);
    }
    ::rtidb::RpcClient<::rtidb::nameserver::NameServer_Stub> name_server_client_1(FLAGS_endpoint);
    name_server_client_1.Init();

    //tablet
    FLAGS_endpoint="127.0.0.1:9931";
    ::rtidb::tablet::TabletImpl* tablet_1 = new ::rtidb::tablet::TabletImpl();
    ok = tablet_1->Init();
    ASSERT_TRUE(ok);
    sleep(2);
    brpc::ServerOptions options1;
    brpc::Server server1;
    if (server1.AddService(tablet_1, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        PDLOG(WARNING, "Fail to add service");
        exit(1);
    }
    if (server1.Start(FLAGS_endpoint.c_str(), &options1) != 0) {
        PDLOG(WARNING, "Fail to start server");
        exit(1);
    }
    ok = tablet_1->RegisterZK();
    ASSERT_TRUE(ok);
    sleep(2);

    // remote ns and tablet
    //ns
    FLAGS_zk_cluster="127.0.0.1:6181";
    FLAGS_zk_root_path="/rtidb3" + GenRand();
    FLAGS_endpoint = "127.0.0.1:9632";
    FLAGS_db_root_path = "/tmp/" + ::rtidb::nameserver::GenRand();

    NameServerImpl* nameserver_2 = new NameServerImpl();
    ok = nameserver_2->Init();
    ASSERT_TRUE(ok);
    sleep(4);
    brpc::ServerOptions options2;
    brpc::Server server2;
    if (server2.AddService(nameserver_2, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        PDLOG(WARNING, "Fail to add service");
        exit(1);
    }
    if (server2.Start(FLAGS_endpoint.c_str(), &options2) != 0) {
        PDLOG(WARNING, "Fail to start server");
        exit(1);
    }
    ::rtidb::RpcClient<::rtidb::nameserver::NameServer_Stub> name_server_client_2(FLAGS_endpoint);
    name_server_client_2.Init();

    // tablet
    FLAGS_endpoint="127.0.0.1:9932";
    ::rtidb::tablet::TabletImpl* tablet_2 = new ::rtidb::tablet::TabletImpl();
    ok = tablet_2->Init();
    ASSERT_TRUE(ok);
    sleep(2);
    brpc::ServerOptions options3;
    brpc::Server server3;
    if (server3.AddService(tablet_2, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        PDLOG(WARNING, "Fail to add service");
        exit(1);
    }
    if (server3.Start(FLAGS_endpoint.c_str(), &options3) != 0) {
        PDLOG(WARNING, "Fail to start server");
        exit(1);
    }
    ok = tablet_2->RegisterZK();
    ASSERT_TRUE(ok);
    sleep(2);
    {
        ::rtidb::nameserver::SwitchModeRequest request;
        ::rtidb::nameserver::GeneralResponse response;
        request.set_sm(kLEADER);
        ok = name_server_client_1.SendRequest(&::rtidb::nameserver::NameServer_Stub::SwitchMode,
                &request, &response, FLAGS_request_timeout_ms, 1);
    }
    {
        std::string alias = "remote";
        std::string msg;
        ::rtidb::nameserver::ClusterAddress add_request;
        ::rtidb::nameserver::GeneralResponse add_response;
        add_request.set_alias(alias);
        add_request.set_zk_path(FLAGS_zk_root_path);
        add_request.set_zk_endpoints(FLAGS_zk_cluster);
        ok = name_server_client_1.SendRequest(&::rtidb::nameserver::NameServer_Stub::AddReplicaCluster,
                &add_request, &add_response, FLAGS_request_timeout_ms, 1);
        ASSERT_TRUE(ok);
        ASSERT_EQ(0, add_response.code());
        sleep(2);
    }
    std::string name = "test" + GenRand();
    {
        CreateTableRequest request;
        GeneralResponse response;
        TableInfo *table_info = request.mutable_table_info();
        table_info->set_name(name);
        TablePartition* partion = table_info->add_table_partition();
        partion->set_pid(1);
        PartitionMeta* meta = partion->add_partition_meta();
        meta->set_endpoint("127.0.0.1:9931");
        meta->set_is_leader(true);
        TablePartition* partion1 = table_info->add_table_partition();
        partion1->set_pid(2);
        PartitionMeta* meta1 = partion1->add_partition_meta();
        meta1->set_endpoint("127.0.0.1:9931");
        meta1->set_is_leader(true);
        ok = name_server_client_1.SendRequest(&::rtidb::nameserver::NameServer_Stub::CreateTable,
                &request, &response, FLAGS_request_timeout_ms, 1);
        ASSERT_TRUE(ok);
        ASSERT_EQ(307, response.code());

        TablePartition* partion2 = table_info->add_table_partition();
        partion2->set_pid(0);
        PartitionMeta* meta2 = partion2->add_partition_meta();
        meta2->set_endpoint("127.0.0.1:9931");
        meta2->set_is_leader(true);
        ok = name_server_client_1.SendRequest(&::rtidb::nameserver::NameServer_Stub::CreateTable,
                &request, &response, FLAGS_request_timeout_ms, 1);
        ASSERT_TRUE(ok);
        ASSERT_EQ(0, response.code());
        sleep(5);
    }
    {
        ::rtidb::nameserver::ShowTableRequest request;
        ::rtidb::nameserver::ShowTableResponse response;
        ok = name_server_client_2.SendRequest(&::rtidb::nameserver::NameServer_Stub::ShowTable,
                &request, &response, FLAGS_request_timeout_ms, 1);
        ASSERT_TRUE(ok);
        ASSERT_EQ(0, response.code());
        ASSERT_EQ(1, response.table_info_size());
        ASSERT_EQ(name, response.table_info(0).name());
        ASSERT_EQ(3, response.table_info(0).table_partition_size());
    }
    {
        ::rtidb::nameserver::DropTableRequest request;
        request.set_name(name);
        ::rtidb::nameserver::GeneralResponse response;
        bool ok = name_server_client_1.SendRequest(&::rtidb::nameserver::NameServer_Stub::DropTable,
                &request, &response, FLAGS_request_timeout_ms, 1);
        ASSERT_TRUE(ok);
        ASSERT_EQ(0, response.code());
        sleep(5);
    }
    {
        ::rtidb::nameserver::ShowTableRequest request;
        ::rtidb::nameserver::ShowTableResponse response;
        ok = name_server_client_2.SendRequest(&::rtidb::nameserver::NameServer_Stub::ShowTable,
                &request, &response, FLAGS_request_timeout_ms, 1);
        ASSERT_TRUE(ok);
        ASSERT_EQ(0, response.code());
        ASSERT_EQ(0, response.table_info_size());
    }
    delete nameserver_1;
    delete tablet_1;
    delete nameserver_2;
    delete tablet_2;
}

TEST_F(NameServerImplRemoteTest, CreateTableInfo) {
    // local ns and tablet
    //ns
    FLAGS_zk_cluster="127.0.0.1:6181";
    FLAGS_zk_root_path="/rtidb3" + GenRand();
    FLAGS_endpoint = "127.0.0.1:9631";
    FLAGS_db_root_path = "/tmp/" + ::rtidb::nameserver::GenRand();

    NameServerImpl* nameserver_1 = new NameServerImpl();
    bool ok = nameserver_1->Init();
    ASSERT_TRUE(ok);
    sleep(4);
    brpc::ServerOptions options;
    brpc::Server server;
    if (server.AddService(nameserver_1, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        PDLOG(WARNING, "Fail to add service");
        exit(1);
    }
    if (server.Start(FLAGS_endpoint.c_str(), &options) != 0) {
        PDLOG(WARNING, "Fail to start server");
        exit(1);
    }
    ::rtidb::RpcClient<::rtidb::nameserver::NameServer_Stub> name_server_client_1(FLAGS_endpoint);
    name_server_client_1.Init();

    //tablet
    FLAGS_endpoint="127.0.0.1:9931";
    ::rtidb::tablet::TabletImpl* tablet_1 = new ::rtidb::tablet::TabletImpl();
    ok = tablet_1->Init();
    ASSERT_TRUE(ok);
    sleep(2);
    brpc::ServerOptions options1;
    brpc::Server server1;
    if (server1.AddService(tablet_1, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        PDLOG(WARNING, "Fail to add service");
        exit(1);
    }
    if (server1.Start(FLAGS_endpoint.c_str(), &options1) != 0) {
        PDLOG(WARNING, "Fail to start server");
        exit(1);
    }
    ok = tablet_1->RegisterZK();
    ASSERT_TRUE(ok);
    sleep(2);

    // remote ns and tablet
    //ns
    FLAGS_zk_cluster="127.0.0.1:6181";
    FLAGS_zk_root_path="/rtidb3" + GenRand();
    FLAGS_endpoint = "127.0.0.1:9632";
    FLAGS_db_root_path = "/tmp/" + ::rtidb::nameserver::GenRand();

    NameServerImpl* nameserver_2 = new NameServerImpl();
    ok = nameserver_2->Init();
    ASSERT_TRUE(ok);
    sleep(4);
    brpc::ServerOptions options2;
    brpc::Server server2;
    if (server2.AddService(nameserver_2, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        PDLOG(WARNING, "Fail to add service");
        exit(1);
    }
    if (server2.Start(FLAGS_endpoint.c_str(), &options2) != 0) {
        PDLOG(WARNING, "Fail to start server");
        exit(1);
    }
    ::rtidb::RpcClient<::rtidb::nameserver::NameServer_Stub> name_server_client_2(FLAGS_endpoint);
    name_server_client_2.Init();

    // tablet
    FLAGS_endpoint="127.0.0.1:9932";
    ::rtidb::tablet::TabletImpl* tablet_2 = new ::rtidb::tablet::TabletImpl();
    ok = tablet_2->Init();
    ASSERT_TRUE(ok);
    sleep(2);
    brpc::ServerOptions options3;
    brpc::Server server3;
    if (server3.AddService(tablet_2, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        PDLOG(WARNING, "Fail to add service");
        exit(1);
    }
    if (server3.Start(FLAGS_endpoint.c_str(), &options3) != 0) {
        PDLOG(WARNING, "Fail to start server");
        exit(1);
    }
    ok = tablet_2->RegisterZK();
    ASSERT_TRUE(ok);
    sleep(2);
    {
        ::rtidb::nameserver::SwitchModeRequest request;
        ::rtidb::nameserver::GeneralResponse response;
        request.set_sm(kLEADER);
        ok = name_server_client_1.SendRequest(&::rtidb::nameserver::NameServer_Stub::SwitchMode,
                &request, &response, FLAGS_request_timeout_ms, 1);
    }
    {
        std::string alias = "remote";
        std::string msg;
        ::rtidb::nameserver::ClusterAddress add_request;
        ::rtidb::nameserver::GeneralResponse add_response;
        add_request.set_alias(alias);
        add_request.set_zk_path(FLAGS_zk_root_path);
        add_request.set_zk_endpoints(FLAGS_zk_cluster);
        ok = name_server_client_1.SendRequest(&::rtidb::nameserver::NameServer_Stub::AddReplicaCluster,
                &add_request, &add_response, FLAGS_request_timeout_ms, 1);
        ASSERT_TRUE(ok);
        ASSERT_EQ(0, add_response.code());
        sleep(2);
    }
    
    ZoneInfo zone_info = GetZoneInfo(nameserver_1);
    std::string name = "test" + GenRand();
    {
        ::rtidb::nameserver::CreateTableInfoRequest request;
        ::rtidb::nameserver::CreateTableInfoResponse response;
        ::rtidb::nameserver::ZoneInfo* zone_info_p = request.mutable_zone_info();
        zone_info_p->CopyFrom(zone_info);
        TableInfo *table_info = request.mutable_table_info();

        table_info->set_name(name);
        TablePartition* partion = table_info->add_table_partition();
        partion->set_pid(1);
        PartitionMeta* meta = partion->add_partition_meta();
        meta->set_endpoint("127.0.0.1:9931");
        meta->set_is_leader(true);
        TablePartition* partion1 = table_info->add_table_partition();
        partion1->set_pid(2);
        PartitionMeta* meta1 = partion1->add_partition_meta();
        meta1->set_endpoint("127.0.0.1:9931");
        meta1->set_is_leader(true);
        TablePartition* partion2 = table_info->add_table_partition();
        partion2->set_pid(0);
        PartitionMeta* meta2 = partion2->add_partition_meta();
        meta2->set_endpoint("127.0.0.1:9931");
        meta2->set_is_leader(true);

        bool ok = name_server_client_2.SendRequest(&::rtidb::nameserver::NameServer_Stub::CreateTableInfo, &request, &response, FLAGS_request_timeout_ms, 3);
        ASSERT_TRUE(ok);
        ASSERT_EQ(0, response.code());
    }
    {
        ::rtidb::nameserver::ShowTableRequest request;
        ::rtidb::nameserver::ShowTableResponse response;
        ok = name_server_client_2.SendRequest(&::rtidb::nameserver::NameServer_Stub::ShowTable,
                &request, &response, FLAGS_request_timeout_ms, 1);
        ASSERT_TRUE(ok);
        ASSERT_EQ(0, response.code());
        ASSERT_EQ(1, response.table_info_size());
        ASSERT_EQ(name, response.table_info(0).name());
        ASSERT_EQ(3, response.table_info(0).table_partition_size());
    }
    
    delete nameserver_1;
    delete tablet_1;
    delete nameserver_2;
    delete tablet_2;
}

}
}

int main(int argc, char** argv) {
    FLAGS_zk_session_timeout = 100000;
    ::testing::InitGoogleTest(&argc, argv);
    srand (time(NULL));
    ::baidu::common::SetLogLevel(::baidu::common::INFO);
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    //FLAGS_db_root_path = "/tmp/" + ::rtidb::nameserver::GenRand();
    return RUN_ALL_TESTS();
}
