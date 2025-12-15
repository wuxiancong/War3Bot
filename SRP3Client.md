graph TD
    %% 样式定义
    classDef input fill:#e1f5fe,stroke:#01579b,stroke-width:2px;
    classDef process fill:#fff9c4,stroke:#fbc02d,stroke-width:2px;
    classDef output fill:#c8e6c9,stroke:#2e7d32,stroke-width:2px;
    classDef error fill:#ffcdd2,stroke:#c62828,stroke-width:4px;

    subgraph 准备数据
    User[用户名 U] & Pass[密码 P] --> Combine[拼接 "USER:PASS"]
    Salt_Net[服务端发来的 Salt] --> Salt_BigInt[转换为 BigInt s]
    end

    subgraph 计算私钥 x (最容易出错!)
    Combine --> HP[计算 SHA1 哈希 H_P]
    Salt_Net & HP --> Concat[拼接: Salt + H_P]
    Concat --> H_Final[计算 SHA1: H_Final]
    H_Final -->|注意: 服务端做了翻转!| X_Val[转换为 BigInt x]
    class X_Val error
    end

    subgraph 计算扰码 u
    KeyB_Net[服务端发来的 Key B] -->|SHA1| H_B[Hash(B)]
    H_B -->|取前4字节| U_Val[转换为 BigInt u]
    end

    subgraph 计算密钥 S 和 K
    KeyB_Net --> KeyB_BigInt[转换为 BigInt B]
    A_Local[本地生成的 a] --> S_Calc
    X_Val & U_Val & KeyB_BigInt --> S_Calc[计算 S = (B - g^x)^(a + ux)]
    S_Calc --> S_Bytes[S 转为 32字节流 (Little Endian)]
    S_Bytes --> Split[拆分奇偶位: Odd / Even]
    Split --> HashInterleave[分别 Hash 并合并]
    HashInterleave --> K_Val[得到 Session Key K]
    end

    subgraph 生成证明 M1
    I_Const[常量 I] & H_User[Hash(User)] & Salt_Net & A_Pub[公钥 A] & KeyB_Net & K_Val --> M1_Calc[拼接并 SHA1]
    M1_Calc --> M1_Out[得到证明 M1]
    class M1_Out output
    end

    %% 连接关系
    X_Val --> S_Calc
    U_Val --> S_Calc
    
    %% 注释
    note1[x 的构造必须匹配服务端:<br>1. SHA1 结果本身是内存字节<br>2. 服务端代码将其视为 Little Endian 读取<br>你需要: blockSize=1, false 或 blockSize=4, false 取决于哈希库] 
    note1 -.-> X_Val