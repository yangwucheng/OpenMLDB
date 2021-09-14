/*
 * Copyright 2021 4Paradigm
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com._4paradigm.openmldb.java_sdk_test.common;


import com._4paradigm.openmldb.sdk.SdkOption;
import com._4paradigm.openmldb.sdk.SqlException;
import com._4paradigm.openmldb.sdk.SqlExecutor;
import com._4paradigm.openmldb.sdk.impl.SqlClusterExecutor;
import com._4paradigm.openmldb.test_common.bean.FEDBInfo;
import lombok.Data;
import lombok.extern.slf4j.Slf4j;

/**
 * @author zhaowei
 * @date 2020/6/11 11:28 AM
 */
@Data
@Slf4j
public class FedbClient {

    private SqlExecutor executor;

    public FedbClient(String zkCluster, String zkPath){
        SdkOption option = new SdkOption();
        option.setZkCluster(zkCluster);
        option.setZkPath(zkPath);
        option.setEnableDebug(true);
        option.setSessionTimeout(1000000);
        option.setRequestTimeout(1000000);
        log.info("zkCluster {}, zkPath {}", option.getZkCluster(), option.getZkPath());
        try {
            executor = new SqlClusterExecutor(option);
        } catch (SqlException e) {
            e.printStackTrace();
        }
    }
    public FedbClient(FEDBInfo fedbInfo){
        this(fedbInfo.getZk_cluster(),fedbInfo.getZk_root_path());
    }
}
