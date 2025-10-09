# 说明
## 代码拉取
仓库需要使用到git-lfs

git-lfs安装

```
apt install git-lfs
```

拉取代码
```
git clone https://cloud.listenai.com/CSKG836746/arcs-sdk/arcs-sdk.git --recurse-submodules
```
或

```
git clone https://cloud.listenai.com/CSKG836746/arcs-sdk/arcs-sdk.git
git submodule update --recursive --init
```

## 目录说明
```
.
├── arcs-base       ARCS的最小依赖目录
├── components      存放组件
├── modules         存放第三方模块
├── projects        存放工程代码
├── samples         存放示例代码
├── tests           存放测试代码
├── tools           存放工具或者脚本
├── build.sh        工程构建脚本
```

在开发前请阅读[开发文档](./doc/开发文档.MD)
