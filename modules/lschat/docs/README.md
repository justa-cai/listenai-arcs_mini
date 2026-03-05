# 文档构建说明
采用doxygen+sphinx+readthedocs来构建和托管文档
- doxygen扫描代码注解
- sphinx生成文档
- 导入readthedocs进行文档托管

## sphinx本地构建
扫描代码注解，在根目录下执行如下命令
``` bash
doxygen docs/Doxyfile
```

构建文档，在docs目录下执行如下命令
``` bash
make html
```

启动http服务，浏览器输入`http://127.0.0.1:8000`进行预览
``` bash
sphinx-autobuild source build/html
```
