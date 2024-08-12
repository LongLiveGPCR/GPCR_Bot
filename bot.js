'use strict';

var ws;
function send(data) {
    ws.send(JSON.stringify(data));
}
function Run() {
    ws = new WebSocket("wss://hack.chat/chat-ws")
    ws.onopen = function () {
        document.write("�ɹ����ӵ���������\n");
        send({ cmd: 'join', nick: 'GPCR_Bot', password: 'GPCR1966Bot', channel: 'GPCR' });
    };  
    ws.onmessage = function (e) {
        const data = JSON.parse(e.data);
        document.write(e.data);
        if (data.channel == false) { //��������
            ws.close();
            return;
        }
        var request = new XMLHttpRequest();
        request.open("POST", "http://localhost:8080/", true);
        request.onreadystatechange = function () {
            if (request.readyState === XMLHttpRequest.DONE) {
                if (request.status === 200) {
                    var text = request.responseText;
                    document.write(text);
                    document.write('\n');
                    if (text != "")
                        send({ cmd: "chat", text: text });
                }
            }
        };
        request.send(e.data);
    };
    ws.onclose = function (e) {
        document.write("��������Ͽ����ӣ�\n");
        let timeId = setInterval(function () {
            document.write("��������...\n");
            if (ws.readyState === WebSocket.CLOSED) {
                Run();
                clearInterval(timeId);
            }
        }, 40000);
    };
}
Run();