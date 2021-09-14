#!/usr/bin/env bash

# Copyright 2021 4Paradigm
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


#bash fedb-sdk-test-java.sh -b SRC -c test_all.xml -d cluster -l 0
#-b SRC表示从源码进行编译，会从github上下载代码然后进行编译，PKG表示直接从github上下载压缩包部署
#-c 执行的suite_xml,决定了跑哪些case
#-d 部署模式，有cluster和standalone两种，默认cluster
#-l 测试的case级别，有0，1，2，3，4，5六个级别，默认为0，也可以同时跑多个级别的case，例如：1,2,3,4,5

while getopts ":b:c:d:l:" opt
do
   case $opt in
        b)
        echo "参数b的值:$OPTARG"
        BUILD_MODE=$OPTARG
        ;;
        c)
        echo "参数c的值:$OPTARG"
        CASE_XML=$OPTARG
        ;;
        d)
        echo "参数d的值:$OPTARG"
        DEPLOY_MODE=$OPTARG
        ;;
        l) echo "参数l的值:$OPTARG"
        CASE_LEVEL=$OPTARG
        ;;
        ?) echo "未知参数"
           exit 1
        ;;
   esac
done
if [[ "${BUILD_MODE}" == "" ]]; then
    BUILD_MODE="PKG"
fi
if [[ "${CASE_XML}" == "" ]]; then
    CASE_XML="test_all.xml"
fi
if [[ "${DEPLOY_MODE}" == "" ]]; then
    DEPLOY_MODE="cluster"
fi
if [[ "${CASE_LEVEL}" == "" ]]; then
    CASE_LEVEL="0"
fi

echo "BUILD_MODE:${BUILD_MODE}"
echo "CASE_XML:${CASE_XML}"
echo "DEPLOY_MODE:${DEPLOY_MODE}"
echo "CASE_LEVEL:${CASE_LEVEL}"

ROOT_DIR=$(pwd)
# 安装wget
yum install -y wget
yum install -y  net-tools
ulimit -c unlimited
echo "ROOT_DIR:${ROOT_DIR}"
source steps/read_properties.sh
sh steps/download-case.sh "${CASE_BRANCH}"
# 从源码编译
if [[ "${BUILD_MODE}" == "SRC" ]]; then
    sh steps/build-fedb.sh
    FEDB_SDK_VERSION=$(more OpenMLDB/src/sdk/java/pom.xml | grep "<version>.*</version>" | head -1 | sed 's#.*<version>\(.*\)</version>.*#\1#')
fi
echo "FEDB_SDK_VERSION:${FEDB_SDK_VERSION}"
echo "FEDB_SERVER_VERSION:${FEDB_SERVER_VERSION}"
echo "FEDB_VERSIONS:${FEDB_VERSIONS}"
# modify config
sh steps/modify_suite_pom_by_restful.sh "${CASE_XML}" "${DEPLOY_MODE}" "${FEDB_SDK_VERSION}" "${BUILD_MODE}" "${FEDB_SERVER_VERSION}"
# install jar
cd java/hybridsql-test || exit
mvn clean install -Dmaven.test.skip=true
cd "${ROOT_DIR}" || exit
# run case
cd "${ROOT_DIR}"/java/hybridsql-test/fedb-restful-test/ || exit
mvn clean test -DsuiteXmlFile=test_suite/"${CASE_XML}" -DcaseLevel=$CASE_LEVEL
