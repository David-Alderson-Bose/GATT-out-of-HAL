
var displayCounter = 1;


function handshake() {
    var msg = "GET |";
    m_Websocket.send(msg);
}


document.onreadystatechange = function() {
    if(document.readyState === 'complete') {
        var m_Websocket;
        //var stageCounter = 2;
        var stageValueElement = document.getElementById("stageValue");
        var volumeValueElement = document.getElementById("volumeValue");

        var wsUrl = null;
        var protocol = "riviera";
        var hostname = "127.0.0.1";
        var port = "8084";
        wsUrl = "ws://" + hostname + ":" + port +"?product=demoDinghy";

        m_Websocket = new WebSocket(wsUrl);

        m_Websocket.onopen = function() {
            var msg = "GET |";
            m_Websocket.send(msg);
        };

        m_Websocket.onmessage = function(ev) {

            try {
                var msgObj = JSON.parse(ev.data);
                var action;
                if(msgObj.header && msgObj.header.method && msgObj.header.method.toLowerCase() === 'notify')
                {
                    action = 'notification';
                }
                else if(msgObj.header)
                {
                    action = msgObj.header.resource;
                }
                else
                {
                    action = 'unknown';
                }
                
                switch(action) {
                    //case '/system/info':
                    //    stageValueElement.innerHTML = msgObj.body;
                    //    break;
                    case 'notification':
                        if(msgObj.header.resource === "/rubeusDemo/sleepPlanState") {
                            stageValueElement.innerHTML = msgObj.body.value;
                        }
                        else if(msgObj.header.resource === "/rubeusDemo/demoCounter") {
                            console.log("skipping this for now");
                        }
                        else if(msgObj.header.resource === "/audio/volume") {
                            volumeValueElement.innerHTML = msgObj.body.value;
                        }
                        break;
                    //case '/network/ping':
                    //    stageValueElement.innerHTML = msgObj.body;
                    //    break;
                    default:
                        // stageValueElement.innerHTML = "switch default";
                        console.log("defaulted")
                        break;
                }
            } catch(e) {
                //stageValueElement.innerHTML = e.message;            
                console.log(e.message);
            }
        };
    }        
}


