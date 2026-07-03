# Unit Tests

这些 Unity 单元测试目前**未接入构建**:它们仍引用旧版(重写前)模块内部 API,
需要先按 `main/` 中的现行接口重写,再通过 ESP-IDF 的组件 `test/` 目录或
`unit-test-app` 方式接入。CI 目前只做固件编译与打包,不运行这些测试。
