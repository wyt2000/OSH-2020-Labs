# 实验三得分 14.5 / 16 分

源提交：[`687dcf8c12dff29e0e7c75a56195d6d23eff1397`](https://github.com/wyt2000/OSH-2020-Labs/tree/687dcf8c12dff29e0e7c75a56195d6d23eff1397)

- 奇怪的 recv 方式（每次 1 字节）（-0.5 分）
- `pthread_cancel` `handle_send` 线程可能导致程序终止（C++11 标准要求析构函数 noexcept，而 `pthread_cancel` 取消线程的方式就是使用异常，若 `handle_send` 函数中 `string s` 析构时刚好线程被取消，则会导致程序终止）（-1 分）
- 合计：14.5 / 16 分
