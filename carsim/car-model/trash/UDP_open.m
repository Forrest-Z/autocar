function UDP_open()
    global send_buf
%    global rece_buf
    global u
%     send_buf = 'hello';
%     rece_buf = '';
    %����ǰ�ȹر�
    echoudp('off');
    %���ñ��ض˿�
    echoudp('on',8099);
    %����Զ��IP��˿�
    Rrmotehost = '192.168.174.1';
    u = udp(Rrmotehost,8088);
    %���ý��յȴ�ʱ��
    set(u,'TimeOut',0.1);
    set(u,'InputBufferSize',1000);
    fopen(u);
    %���뻺�渳ֵ�������������̸߳�ֵ
    % send_buf = 'hello';
    delete(timerfind)   % ����ǰɾ�����еĶ�ʱ�� 
    send_timer = timer('StartDelay',1,'TimerFcn',@send_callback,'Period',2,'ExecutionMode','fixedRate');
    rece_timer = timer('StartDelay',1,'TimerFcn',@rece_callback,'Period',2,'ExecutionMode','fixedRate');
    fwrite(u,send_buf);
    start([send_timer,rece_timer])
end

