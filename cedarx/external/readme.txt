
# Author: chenxiaochuan
# Date:   2014-06-18

    EXTERNALĿ¼���LIBRARY�¸�������ģ���������ⲿ�����ͷ�ļ���Ŀǰ����:
        1. openssl;
        2. zlib;
        3. libxml2;
�����ǹ�����δ���Щ�ⲿ���Դ��������ɱ�Ŀ¼��ŵĿ��ļ���˵����

1. openssl
    a. ����openssl 1.0.1e�汾��Դ�룬��ѹ��
    b. �ڽ�ѹĿ¼��ִ��./config no-asm shared --prefix=/home/cxc/openssl�������ã�
       ����--prefix=/home/cxc/opensslָ�����ͷ�ļ������·���������ʵ������޸ġ�
    c. �޸�Makefile�ļ�������
        CC= gcc һ���޸�Ϊ
            CC= arm-none-linux-gnueabi-gcc
            ���ʹ��arm-linux-gnueabihf-��������������޸�Ϊ
            CC= arm-linux-gnueabihf-gcc
        AR= ar $(ARFLAGS) r һ���޸�Ϊ
            AR= arm-none-linux-gnueabi-ar $(ARFLAGS) r
            ���ʹ��arm-linux-gnueabihf-��������������޸�Ϊ
            AR= arm-linux-gnueabihf-ar $(ARFLAGS) r
        RANLIB= /usr/bin/ranlib һ���޸�Ϊ
            RANLIB= arm-none-linux-gnueabi-ranlib
            ���ʹ��arm-linux-gnueabihf-��������������޸�Ϊ
            RANLIB= arm-linux-gnueabihf-ranlib
        �����CFLAG�����ð���-m64ѡ���Ŀ�����Ϊ32λ��ɾ����ѡ��
    d. ��Դ��Ŀ¼��ִ��make���ȴ�������ɺ�ִ��make install
    e. make install��/home/cxc/opensslĿ¼�������ͷ�ļ������俽������Ҫ���õĵط���

2. zlib
    a. ����zlib 1.2.8�汾��Դ�룬��ѹ��
    b. ��shell��ִ��
            export CC=arm-none-linux-gnueabi-gcc
       ���ʹ��arm-linux-gnueabihf-�������������ִ��
            export CC=arm-linux-gnueabihf-gcc
    c. �ڽ�ѹĿ¼��ִ��
            ./configure --prefix=/home/cxc/zlib
       ����/home/cxc/zlibָ�����ͷ�ļ������·���������ʵ������޸ģ�
    d. ��Դ��Ŀ¼��ִ��make���ȴ�������ɺ�ִ��make install
    e. make install��/home/cxc/zlibĿ¼�������ͷ�ļ������俽������Ҫ���õĵط���

2. libxml2
    a. ����libxml2 v2.7.8�汾Դ�룻
    b. ��Դ��Ŀ¼��ִ��
            ./audogen.sh
    c. ��Դ��Ŀ¼��ִ��
            ./configure --host=arm-none-linux-gnueabi CC=arm-none-linux-gnueabi-gcc \
            LD=arm-none-linux-gnueabi-ld RANLIB=arm-none-linux-gnueabi-ranlib \
            --prefix=/home/AL3/libxml2 --without-zlib
       ���ʹ��arm-linux-gnueabihf-�������������ִ��
            ./configure --host=arm-linux-gnueabihf CC=arm-linux-gnueabihf-gcc \
            LD=arm-linux-gnueabihf-ld RANLIB=arm-linux-gnueabihf-ranlib \
            --prefix=/home/AL3/libxml2 --without-zlib
       ����/home/cxc/zlibָ�����ͷ�ļ������·���������ʵ������޸ģ�
    d. ��Դ��Ŀ¼��ִ��make���ȴ�������ɺ�ִ��make install
    e. make install��/home/cxc/libxml2Ŀ¼�������ͷ�ļ������俽������Ҫ���õĵط���
