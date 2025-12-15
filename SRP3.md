sequenceDiagram
    participant C as 客户端 (BnetBot)
    participant S as 服务端 (War3Server)
    
    Note over C,S: 预备阶段：TCP连接建立，握手完成

    %% 步骤 1
    Note over C: 1. 初始化<br/>生成随机私钥 a<br/>计算公钥 A = g^a % N
    C->>S: 发送 [SID_AUTH_ACCOUNTLOGON] (0x53)<br/>Payload: [32字节 A] + [用户名]
    
    %% 步骤 2
    Note over S: 2. 查找用户<br/>取出数据库中的 Salt(s) 和 Verifier(v)<br/>生成随机私钥 b<br/>计算公钥 B = (v + g^b) % N
    S->>C: 回复 [SID_AUTH_ACCOUNTLOGON] (0x53)<br/>Payload: [32字节 Salt] + [32字节 B]
    
    %% 步骤 3
    rect rgb(240, 248, 255)
    Note over C: 3. 核心计算 (客户端)<br/>a. 接收 s, B (小端序)<br/>b. 计算 x = H(s, H(P))<br/>c. 计算 u = H(B)<br/>d. 计算 S = (B - g^x)^(a + ux)<br/>e. 计算 K = HashInterleave(S)<br/>f. 计算 M1 = H(I, H(U), s, A, B, K)
    end
    
    C->>S: 发送 [SID_AUTH_ACCOUNTLOGONPROOF] (0x54)<br/>Payload: [20字节 M1]
    
    %% 步骤 4
    rect rgb(255, 250, 240)
    Note over S: 4. 验证 (服务端)<br/>a. 计算 u, S, K<br/>b. 计算期望的 M1'<br/>c. 对比 M1 == M1'<br/>d. 若成功，计算 M2 = H(A, M1, K)
    end
    
    alt 验证成功
        S->>C: 回复 [SID_AUTH_ACCOUNTLOGONPROOF] (0x54)<br/>Payload: [错误码 0] + [20字节 M2]
        Note over C: 登录成功<br/>校验 M2 (可选)
    else 验证失败 (你当前遇到的情况)
        S->>C: 回复 [SID_AUTH_ACCOUNTLOGONPROOF] (0x54)<br/>Payload: [错误码 0x02 (密码错误)]
        Note over C: 登录失败
    end