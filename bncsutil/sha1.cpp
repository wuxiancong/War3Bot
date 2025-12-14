/**
 * @file shar1.cpp
 * @brief SHA-1 哈希算法
 * 作用：
 * 实现了标准的 SHA-1 加密哈希算法。
 * 主要功能：
 * 1. CheckRevision：将 .exe 和 .dll 的文件内容计算为哈希值发送给服务器。
 * 2. 账号登录：在 NLS (SRP) 登录流程中，用于对账号密码进行加密运算，确保密码不以明文传输。
*/

#include <cstdint>
#include "bncsutil/sha1.h"

/*
 * 定义 SHA1 循环左移宏
 */
#define SHA1CircularShift(bits,word) \
                (((word) << (bits)) | ((word) >> (32-(bits))))

/* 本地函数原型 */
void SHA1PadMessage(SHA1Context*);
void SHA1ProcessMessageBlock(SHA1Context*);

/*
 * SHA1Reset
 *
 * 描述:
 *     此函数将初始化 SHA1Context，为计算新的 SHA1 消息摘要做准备。
 *
 * 参数:
 *     context: [输入/输出]
 *         要重置的上下文。
 *
 * 返回值:
 *     sha 错误代码。
 *
 */
int SHA1Reset(SHA1Context *context)
{
    if (!context)
    {
        return shaNull;
    }

    context->Length_Low             = 0;
    context->Length_High            = 0;
    context->Message_Block_Index    = 0;

    context->Intermediate_Hash[0]   = 0x67452301;
    context->Intermediate_Hash[1]   = 0xEFCDAB89;
    context->Intermediate_Hash[2]   = 0x98BADCFE;
    context->Intermediate_Hash[3]   = 0x10325476;
    context->Intermediate_Hash[4]   = 0xC3D2E1F0;

    context->Computed   = 0;
    context->Corrupted  = 0;

    return shaSuccess;
}

/*
 * SHA1Result
 *
 * 描述:
 *     此函数将返回 160 位消息摘要到调用者提供的 Message_Digest 数组中。
 *     注意：哈希的第一个字节存储在第 0 个元素中，哈希的最后一个字节存储在第 19 个元素中。
 *
 * 参数:
 *     context: [输入/输出]
 *         用于计算 SHA-1 哈希的上下文。
 *     Message_Digest: [输出]
 *         返回摘要的位置。
 *
 * 返回值:
 *     sha 错误代码。
 *
 */
// 【关键修复 2】uint8_t 现在可以被识别了
int SHA1Result( SHA1Context *context,
                uint8_t Message_Digest[SHA1HashSize])
{
    int i;

    if (!context || !Message_Digest)
    {
        return shaNull;
    }

    if (context->Corrupted)
    {
        return context->Corrupted;
    }

    if (!context->Computed)
    {
        SHA1PadMessage(context);
        for(i=0; i<64; ++i)
        {
            /* 消息可能包含敏感信息，将其清除 */
            context->Message_Block[i] = 0;
        }
        context->Length_Low = 0;    /* 并清除长度 */
        context->Length_High = 0;
        context->Computed = 1;

    }

    for(i = 0; i < SHA1HashSize; ++i)
    {
        Message_Digest[i] = context->Intermediate_Hash[i>>2]
                            >> 8 *( 3 - ( i & 0x03 ) );
    }

    return shaSuccess;
}

/*
 * SHA1Input
 *
 * 描述:
 *     此函数接受一个字节数组作为消息的下一部分。
 *
 * 参数:
 *     context: [输入/输出]
 *         要更新的 SHA 上下文。
 *     message_array: [输入]
 *         代表消息下一部分的字符数组。
 *     length: [输入]
 *         message_array 中消息的长度。
 *
 * 返回值:
 *     sha 错误代码。
 *
 */
int SHA1Input(    SHA1Context   *context,
                  const uint8_t *message_array,
                  unsigned       length)
{
    if (!length)
    {
        return shaSuccess;
    }

    if (!context || !message_array)
    {
        return shaNull;
    }

    if (context->Computed)
    {
        context->Corrupted = shaStateError;

        return shaStateError;
    }

    if (context->Corrupted)
    {
         return context->Corrupted;
    }
    while(length-- && !context->Corrupted)
    {
    context->Message_Block[context->Message_Block_Index++] =
                    (*message_array & 0xFF);

    context->Length_Low += 8;
    if (context->Length_Low == 0)
    {
        context->Length_High++;
        if (context->Length_High == 0)
        {
            /* 消息太长了 */
            context->Corrupted = 1;
        }
    }

    if (context->Message_Block_Index == 64)
    {
        SHA1ProcessMessageBlock(context);
    }

    message_array++;
    }

    return shaSuccess;
}

/*
 * SHA1ProcessMessageBlock
 *
 * 描述:
 *     此函数将处理存储在 Message_Block 数组中的消息的下一个 512 位。
 *
 * 参数:
 *     无。
 *
 * 返回值:
 *     无。
 *
 * 备注:
 *     此代码中的许多变量名称，尤其是单字符名称，
 *     是因为它们是出版物中使用的名称。
 *
 *
 */
void SHA1ProcessMessageBlock(SHA1Context *context)
{
    // 【关键修复 3】uint32_t 现在可以被识别了
    const uint32_t K[] =    {       /* SHA-1 中定义的常量  */
                            0x5A827999,
                            0x6ED9EBA1,
                            0x8F1BBCDC,
                            0xCA62C1D6
                            };
    int           t;                 /* 循环计数器               */
    uint32_t      temp;              /* 临时字值       */
    uint32_t      W[80];             /* 字序列              */
    uint32_t      A, B, C, D, E;     /* 字缓冲区               */

    /*
     * 初始化数组 W 中的前 16 个字
    */
    for(t = 0; t < 16; t++)
    {
        W[t] = context->Message_Block[t *4] << 24;
        W[t] |= context->Message_Block[t *4 + 1] << 16;
        W[t] |= context->Message_Block[t *4 + 2] << 8;
        W[t] |= context->Message_Block[t *4 + 3];
    }

    for(t = 16; t < 80; t++)
    {
       W[t] = SHA1CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
    }

    A = context->Intermediate_Hash[0];
    B = context->Intermediate_Hash[1];
    C = context->Intermediate_Hash[2];
    D = context->Intermediate_Hash[3];
    E = context->Intermediate_Hash[4];

    for(t = 0; t < 20; t++)
    {
        temp =  SHA1CircularShift(5,A) +
                ((B & C) | ((~B) & D)) + E + W[t] + K[0];
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);

        B = A;
        A = temp;
    }

    for(t = 20; t < 40; t++)
    {
        temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }

    for(t = 40; t < 60; t++)
    {
        temp = SHA1CircularShift(5,A) +
               ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }

    for(t = 60; t < 80; t++)
    {
        temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }

    context->Intermediate_Hash[0] += A;
    context->Intermediate_Hash[1] += B;
    context->Intermediate_Hash[2] += C;
    context->Intermediate_Hash[3] += D;
    context->Intermediate_Hash[4] += E;

    context->Message_Block_Index = 0;
}

/*
 * SHA1PadMessage
 *
 * 描述:
 *     根据标准，消息必须填充到偶数 512 位。
 *     第一个填充位必须是 '1'。最后 64 位表示原始消息的长度。
 *     中间的所有位都应为 0。
 *     此函数将根据这些规则通过相应填充 Message_Block 数组来填充消息。
 *     它还将适当地调用提供的 ProcessMessageBlock 函数。
 *     当它返回时，可以假设消息摘要已计算完成。
 *
 * 参数:
 *     context: [输入/输出]
 *         要填充的上下文
 *     ProcessMessageBlock: [输入]
 *         相应的 SHA*ProcessMessageBlock 函数
 * 返回值:
 *     无。
 *
 */

void SHA1PadMessage(SHA1Context *context)
{
    /*
     * 检查当前消息块是否太小，无法容纳初始填充位和长度。
     * 如果是这样，我们将填充该块，处理它，然后继续填充到第二个块中。
    */
    if (context->Message_Block_Index > 55)
    {
        context->Message_Block[context->Message_Block_Index++] = 0x80;
        while(context->Message_Block_Index < 64)
        {
            context->Message_Block[context->Message_Block_Index++] = 0;
        }

        SHA1ProcessMessageBlock(context);

        while(context->Message_Block_Index < 56)
        {
            context->Message_Block[context->Message_Block_Index++] = 0;
        }
    }
    else
    {
        context->Message_Block[context->Message_Block_Index++] = 0x80;
        while(context->Message_Block_Index < 56)
        {

            context->Message_Block[context->Message_Block_Index++] = 0;
        }
    }

    /*
     * 将消息长度存储为最后 8 个字节
    */
    context->Message_Block[56] = context->Length_High >> 24;
    context->Message_Block[57] = context->Length_High >> 16;
    context->Message_Block[58] = context->Length_High >> 8;
    context->Message_Block[59] = context->Length_High;
    context->Message_Block[60] = context->Length_Low >> 24;
    context->Message_Block[61] = context->Length_Low >> 16;
    context->Message_Block[62] = context->Length_Low >> 8;
    context->Message_Block[63] = context->Length_Low;

    SHA1ProcessMessageBlock(context);
}
