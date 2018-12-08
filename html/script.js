var mode = 'null';
var gpio_list = [0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16];
var wsQueue = [];
var wsBusy = false;
var wsTimerId;

// json with effect definitions
var effectInfo;

// Default modal properties
$.fn.modal.Constructor.DEFAULTS.backdrop = 'static';
$.fn.modal.Constructor.DEFAULTS.keyboard = false;

// jQuery doc ready
$(function() {
    // Menu navigation for single page layout

    $('ul.navbar-nav li a').click(function() {
        // Highlight proper navbar item
        $('.nav li').removeClass('active');
        $(this).parent().addClass('active');

        // Show the proper menu div
        $('.mdiv').addClass('hidden');
        $($(this).attr('href')).removeClass('hidden');

        // kick start the live stream
	if ($(this).attr('href') == "#stream") {
            wsEnqueue('T9');
	}

        // Collapse the menu on smaller screens
        $('#navbar').removeClass('in').attr('aria-expanded', 'false');
        $('.navbar-toggle').attr('aria-expanded', 'false');

        // Firmware selection and upload
        $('#efu').change(function () {
            $('#updatefw').submit();
            $('#update').modal();
        });

        // Restore config selection and upload
        $('#restoreFile').change(function () {
            $('#restoreForm').submit();
            $('#update').modal();
        });

        // Color Picker
        $('.color').colorPicker({
            buildCallback: function($elm) {
                var colorInstance = this.color;
                var colorPicker = this;

                $elm.append('<div class="cp-memory">' +
                    '<div style="background-color: #FFFFFF";></div>' +
                    '<div style="background-color: #FF0000";></div>' +
                    '<div style="background-color: #FFFF00";></div>' +
                    '<div style="background-color: #00FF00";></div>' +
                    '<div style="background-color: #00FFFF";></div>' +
                    '<div style="background-color: #0000FF";></div>' +
                    '<div style="background-color: #FF00FF";></div>' +
                    '<div style="background-color: #000000";></div>').
                on('click', '.cp-memory div', function(e) {
                    var $this = $(this);

                    if (this.className) {
                        $this.parent().prepend($this.prev()).children().eq(0).
                            css('background-color', '#' + colorInstance.colors.HEX);
                    } else {
                        colorInstance.setColor($this.css('background-color'));
                        colorPicker.render();
                    }
                });

                this.$colorPatch = $elm.prepend('<div class="cp-disp">').find('.cp-disp');
            },

            cssAddon:
                '.cp-memory {margin-bottom:6px; clear:both;}' +
                '.cp-memory div {float:left; width:12.5%; height:40px;' +
                'background:rgba(0,0,0,1); text-align:center; line-height:40px;}' +
                '.cp-disp{padding:10px; margin-bottom:6px; font-size:19px; height:40px; line-height:20px}' +
                '.cp-xy-slider{width:200px; height:200px;}' +
                '.cp-xy-cursor{width:16px; height:16px; border-width:2px; margin:-8px}' +
                '.cp-z-slider{height:200px; width:40px;}' +
                '.cp-z-cursor{border-width:8px; margin-top:-8px;}',

            opacity: false,

            renderCallback: function($elm, toggled) {
                var colors = this.color.colors.RND;
                var json = {
                        'r': colors.rgb.r,
                        'g': colors.rgb.g,
                        'b': colors.rgb.b
                    };

                this.$colorPatch.css({
                    backgroundColor: '#' + colors.HEX,
                    color: colors.RGBLuminance > 0.22 ? '#222' : '#ddd'
                }).text(this.color.toString($elm._colorMode)); // $elm.val();

                var tmode = $('#tmode option:selected').val();
                if (typeof effectInfo[tmode].wsTCode !== 'undefined') {
                    if (effectInfo[tmode].hasColor) {
                        wsEnqueue( effectInfo[tmode].wsTCode + JSON.stringify(json) );
                    }
                }
            }
        });

        // Set page event feeds
        feed();
    });

    // Reverse checkbox
    $('.reverse').click(function() {
      // On click(), the new checkbox state has already been set
      var json = { 'reverse': $(this).prop('checked') };
      var tmode = $('#tmode option:selected').val();

      if (typeof effectInfo[tmode].wsTCode !== 'undefined') {
          if (effectInfo[tmode].hasReverse) {
              wsEnqueue( effectInfo[tmode].wsTCode + JSON.stringify(json) );
          }
      }
    });

    // Mirror checkbox
    $('.mirror').click(function() {
      // On click(), the new checkbox state has already been set
      var json = { 'mirror': $(this).prop('checked') };
      var tmode = $('#tmode option:selected').val();

      if (typeof effectInfo[tmode].wsTCode !== 'undefined') {
          if (effectInfo[tmode].hasMirror) {
              wsEnqueue( effectInfo[tmode].wsTCode + JSON.stringify(json) );
          }
      }
    });

    // AllLeds checkbox
    $('.allleds').click(function() {
      // On click(), the new checkbox state has already been set
      var json = { 'allleds': $(this).prop('checked') };
      var tmode = $('#tmode option:selected').val();

      if (typeof effectInfo[tmode].wsTCode !== 'undefined') {
          if (effectInfo[tmode].hasAllLeds) {
              wsEnqueue( effectInfo[tmode].wsTCode + JSON.stringify(json) );
          }
      }
    });

    // Test mode toggles
    $('#tmode').change(hideShowTestSections());

    // DHCP field toggles
    $('#dhcp').click(function() {
        if ($(this).is(':checked')) {
            $('.dhcp').addClass('hidden');
       } else {
            $('.dhcp').removeClass('hidden');
       }
    });

    // MQTT field toggles
    $('#mqtt').click(function() {
        if ($(this).is(':checked')) {
            $('.mqtt').removeClass('hidden');
       } else {
            $('.mqtt').addClass('hidden');
       }
    });

    $('#p_gammaVal').change(function() {
            sendGamma();
    });
    $('#p_briteVal').change(function() {
            sendGamma();
    });

    // Gamma graph
    $('#showgamma').click(function() {
        if ($(this).is(':checked')) {
            $('.gammagraph').removeClass('hidden');
       } else {
            $('.gammagraph').addClass('hidden');
       }
    });

    // PWM field toggles
    $('#pwm_enabled').click(function() {
        if ($(this).is(':checked')) {
            $('.pwm').removeClass('hidden');
       } else {
            $('.pwm').addClass('hidden');
       }
    });

    // Pixel type toggles
    $('#p_type').change(function() {
        if ($('select[name_type]').val() == '1')
            $('#p_color').prop('disabled', true);
        else
            $('#p_color').prop('disabled', false);
    });

    // Serial protocol toggles
    $('#s_proto').change(function() {
        if ($('select[name=s_proto]').val() == '0')
            $('#s_baud').prop('disabled', true);
        else
            $('#s_baud').prop('disabled', false);
    });

    // Hostname, SSID, and Password validation
    $('#hostname').keyup(function() {
        wifiValidation();
    });
    $('#ssid').keyup(function() {
        wifiValidation();
    });
    $('#password').keyup(function() {
        wifiValidation();
    });
    $('#ap').change(function () {
        wifiValidation();
    });
    $('#dhcp').change(function () {
        wifiValidation();
    });
    $('#gateway').keyup(function () {
        wifiValidation();
    });
    $('#ip').keyup(function () {
        wifiValidation();
    });
    $('#netmask').keyup(function () {
        wifiValidation();
    });
    $('#udp_port').keyup(function () {
        wifiValidation();
    });
    $('#udp_enabled').change(function () {
        wifiValidation();
    });

    canvas = document.getElementById("canvas");
    ctx = canvas.getContext("2d");
    ctx.font = "20px Arial";
    ctx.textAlign = "center";

    // autoload tab based on URL hash
    var hash = window.location.hash;
    hash && $('ul.navbar-nav li a[href="' + hash + '"]').click();

});

function wifiValidation() {
    var WifiSaveDisabled = false;
    var re = /^([a-zA-Z0-9][a-zA-Z0-9][a-zA-Z0-9\-.]*[a-zA-Z0-9.])$/;
    if (re.test($('#hostname').val()) && $('#hostname').val().length <= 255) {
        $('#fg_hostname').removeClass('has-error');
        $('#fg_hostname').addClass('has-success');
    } else {
        $('#fg_hostname').removeClass('has-success');
        $('#fg_hostname').addClass('has-error');
        WifiSaveDisabled = true;
    }
    if ($('#ssid').val().length <= 32) {
        $('#fg_ssid').removeClass('has-error');
        $('#fg_ssid').addClass('has-success');
    } else {
        $('#fg_ssid').removeClass('has-success');
        $('#fg_ssid').addClass('has-error');
        WifiSaveDisabled = true;
    }
    if ($('#password').val().length <= 64) {
        $('#fg_password').removeClass('has-error');
        $('#fg_password').addClass('has-success');
    } else {
        $('#fg_password').removeClass('has-success');
        $('#fg_password').addClass('has-error');
        WifiSaveDisabled = true;
    }
    if ($('#dhcp').prop('checked') === false) {
        var iptest = new RegExp(''
        + /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
        + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
        + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
        + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.source
        );

        if (iptest.test($('#ip').val())) {
            $('#fg_ip').removeClass('has-error');
            $('#fg_ip').addClass('has-success');
        } else {
            $('#fg_ip').removeClass('has-success');
            $('#fg_ip').addClass('has-error');
            WifiSaveDisabled = true;
        }
        if (iptest.test($('#netmask').val())) {
            $('#fg_netmask').removeClass('has-error');
            $('#fg_netmask').addClass('has-success');
        } else {
            $('#fg_netmask').removeClass('has-success');
            $('#fg_netmask').addClass('has-error');
            WifiSaveDisabled = true;
        }
        if (iptest.test($('#gateway').val())) {
            $('#fg_gateway').removeClass('has-error');
            $('#fg_gateway').addClass('has-success');
        } else {
            $('#fg_gateway').removeClass('has-success');
            $('#fg_gateway').addClass('has-error');
            WifiSaveDisabled = true;
        }
    }
    if ( ($('#udp_port').val() >= 1) && ($('#udp_port').val() <= 64000) ) {
        $('#fg_udpport').removeClass('has-error');
        $('#fg_udpport').addClass('has-success');
    } else {
        $('#fg_udpport').removeClass('has-success');
        $('#fg_udpport').addClass('has-error');
        WifiSaveDisabled = true;
    }
    $('#btn_wifi').prop('disabled', WifiSaveDisabled);
}

// Page event feeds
function feed() {
    if ($('#home').is(':visible')) {
        wsEnqueue('XJ');

        setTimeout(function() {
            feed();
        }, 1000);
    }
}

function param(name) {
    return (location.search.split(name + '=')[1] || '').split('&')[0];
}

// WebSockets
function wsConnect() {
    if ('WebSocket' in window) {

// accept ?target=10.0.0.123 to make a WS connection to another device
        if (target = param('target')) {
// 
        } else {
            target = document.location.host;
        }

        // Open a new web socket and set the binary type
        ws = new WebSocket('ws://' + target + '/ws');
        ws.binaryType = 'arraybuffer';

        ws.onopen = function() {
            $('#wserror').modal('hide');
            wsEnqueue('E1'); // Get html elements
            wsEnqueue('G1'); // Get Config
            wsEnqueue('G2'); // Get Net Status
            wsEnqueue('G3'); // Get Effect Info
            wsEnqueue('G4'); // Get Gamma Table

            feed();
        };

        ws.onmessage = function (event) {
            if(typeof event.data === "string") {
                var cmd = event.data.substr(0, 2);
                var data = event.data.substr(2);
                switch (cmd) {
                case 'E1':
                    getElements(data);
                    break;
                case 'G1':
                    getConfig(data);
                    break;
                case 'G2':
                    getConfigStatus(data);
                    break;
                case 'G3':
                    getEffectInfo(data);
                    break;
                case 'G4':
                    refreshGamma(data);
                    break;
                case 'S1':
                    setConfig(data);
                    reboot();
                    break;
                case 'S2':
                    setConfig(data);
                    break;
                case 'S3':
                    snackSave();
                    break;
                case 'S4':
                    break;
                case 'XJ':
                    getJsonStatus(data);
                    break;
                case 'X6':
                    showReboot();
                    break;
                case 'OK':
                    break;
                default:
                    console.log('Unknown Command: ' + event.data);
                    break;
                }
            } else {
                streamData= new Uint8Array(event.data);
                drawStream(streamData);
                if ($('#stream').is(':visible')) wsEnqueue('T9');
//                if (!$('#tmode option:selected').val().localeCompare('t_view')) wsEnqueue('T9');
            }
            wsReadyToSend();
        };

        ws.onclose = function() {
            $('#wserror').modal();
            wsConnect();
        };

    } else {
        alert('WebSockets is NOT supported by your Browser! You will need to upgrade your browser or downgrade to v2.0 of the ESPixelStick firmware.');
    }
}

function wsEnqueue(message) {
    //only add a message to the queue if there isn't already one of the same type already queued, otherwise update the message with the latest request.
    wsQueueIndex=wsQueue.findIndex(wsCheckQueue,message);
    if(wsQueueIndex == -1) {
        //add message
        wsQueue.push(message);
    } else {
        //update message
        wsQueue[wsQueueIndex]=message;
    }
    wsProcessQueue();
}

function wsCheckQueue(value) {
    //messages are of the same type if the first two characters match
    return value.substr(0,2)==this.substr(0,2);
}

function wsProcessQueue() {
    //check if currently waiting for a response
    if(wsBusy) {
        //console.log('WS queue busy : ' + wsQueue);
    } else {
        //set wsBusy flag that we are waiting for a response
        wsBusy=true;
        //get next message from queue.
        message=wsQueue.shift();
        //set timeout to clear flag and try next message if response isn't recieved. Short timeout for message types that don't generate a response.
        if(['T0','T1','T2','T3','T4','T5','T6','T7','X6'].indexOf(message.substr(0,2))) {
            timeout=40;
        } else {
            timeout=2000;
        }
        wsTimerId=setTimeout(wsReadyToSend,timeout);
        //send it.
        //console.log('WS sending ' + message);
        ws.send(message);
    }
}

function wsReadyToSend() {
    clearTimeout(wsTimerId);
    wsBusy=false;
    if(wsQueue.length>0) {
        //send next message
        wsProcessQueue();
    } else {
        //console.log('WS queue empty');
    }
}

function drawStream(streamData) {
    var cols=parseInt($('#v_columns').val());
    var size=Math.floor((canvas.width-20)/cols);
    if($("input[name='viewStyle'][value='RGB']").prop('checked')) {
        maxDisplay=Math.min(streamData.length, (cols*Math.floor((canvas.height-30)/size))*3);
        for (i = 0; i < maxDisplay; i+=3) {
            ctx.fillStyle='rgb(' + streamData[i+0] + ',' + streamData[i+1] + ',' + streamData[i+2] + ')';
            var col=(i/3)%cols;
            var row=Math.floor((i/3)/cols);
            ctx.fillRect(10+(col*size),10+(row*size),size-1,size-1);
        }
    } else {
        maxDisplay=Math.min(streamData.length, (cols*Math.floor((canvas.height-30)/size)));
        for (i = 0; i < maxDisplay; i++) {
            ctx.fillStyle='rgb(' + streamData[i] + ',' + streamData[i] + ',' + streamData[i] + ')';
            var col=(i)%cols;
            var row=Math.floor(i/cols);
            ctx.fillRect(10+(col*size),10+(row*size),size-2,size-2);
        }
    }
    if(streamData.length>maxDisplay) {
        ctx.fillStyle='rgb(204,0,0)';
        ctx.fillRect(0,canvas.height-25,canvas.width,25);
        ctx.fillStyle='rgb(255,255,255)';
        ctx.fillText("Increase number of columns to show all data" , (canvas.width/2), canvas.height-5);
    }

}

function clearStream() {
    if (typeof ctx !== 'undefined') {
     ctx.clearRect(0, 0, canvas.width, canvas.height);
    }
}

function getElements(data) {
    var elements = JSON.parse(data);

    for (var i in elements) {
        for (var j in elements[i]) {
            var opt = document.createElement('option');
            opt.text = j;
            opt.value = elements[i][j];
            document.getElementById(i).add(opt);
        }
    }
}

function getConfig(data) {
    var config = JSON.parse(data);

    // Device and Network config
    $('#title').text('ESPB - ' + config.device.id);
    $('#name').text(config.device.id);
    $('#devid').val(config.device.id);
    $('#ssid').val(config.network.ssid);
    $('#password').val(config.network.passphrase);
    $('#hostname').val(config.network.hostname);
    $('#dhcp').prop('checked', config.network.dhcp);
    if (config.network.dhcp) {
        $('.dhcp').addClass('hidden');
    } else {
        $('.dhcp').removeClass('hidden');
    }
    $('#ap').prop('checked', config.network.ap_fallback);
    $('#ip').val(config.network.ip[0] + '.' +
            config.network.ip[1] + '.' +
            config.network.ip[2] + '.' +
            config.network.ip[3]);
    $('#netmask').val(config.network.netmask[0] + '.' +
            config.network.netmask[1] + '.' +
            config.network.netmask[2] + '.' +
            config.network.netmask[3]);
    $('#gateway').val(config.network.gateway[0] + '.' +
            config.network.gateway[1] + '.' +
            config.network.gateway[2] + '.' +
            config.network.gateway[3]);

    $('#udp_enabled').prop('checked', config.network.udp_enabled);
    $('#udp_port').val(config.network.udp_port);

    // MQTT Config
    $('#mqtt').prop('checked', config.mqtt.enabled);
    if (config.mqtt.enabled) {
        $('.mqtt').removeClass('hidden');
    } else {
        $('.mqtt').addClass('hidden');
    }
    $('#mqtt_ip').val(config.mqtt.ip);
    $('#mqtt_port').val(config.mqtt.port);
    $('#mqtt_user').val(config.mqtt.user);
    $('#mqtt_password').val(config.mqtt.password);
    $('#mqtt_topic').val(config.mqtt.topic);
    $('#mqtt_clean').prop('checked', config.mqtt.clean);

    // E1.31 Config
    $('#universe').val(config.e131.universe);
    $('#universe_limit').val(config.e131.universe_limit);
    $('#channel_start').val(config.e131.channel_start);
    $('#multicast').prop('checked', config.e131.multicast);

    // Output Config
    $('.odiv').addClass('hidden');
    if (config.device.mode & 0x01) {  // Pixel
        mode = 'pixel';
        $('#o_pixel').removeClass('hidden');
        $('#p_count').val(config.e131.channel_count / 3);
        $('#p_type').val(config.pixel.type);
        $('#p_color').val(config.pixel.color);
        $('#p_gamma').prop('checked', config.pixel.gamma);
        $('#p_gammaVal').val(config.pixel.gammaVal);
        $('#p_briteVal').val(config.pixel.briteVal);

//      if(config.e131.channel_count / 3 <8 ) {
//          $('#v_columns').val(config.e131.channel_count / 3);
//      } else if (config.e131.channel_count / 3 <50 ) {
//          $('#v_columns').val(10);
//      } else {
//          $('#v_columns').val(25);
//      }
        $('#v_columns').val(Math.floor(Math.sqrt(config.e131.channel_count/3)));

        $("input[name='viewStyle'][value='RGB']").trigger('click');
        clearStream();

        // Trigger updated elements
        $('#p_type').trigger('click');
        $('#p_count').trigger('change');
    }

    if (config.device.mode & 0x02) {  // Serial
        mode = 'serial';
        $('#o_serial').removeClass('hidden');
        $('#s_count').val(config.e131.channel_count);
        $('#s_proto').val(config.serial.type);
        $('#s_baud').val(config.serial.baudrate);

        if (config.e131.channel_count<=64 ) {
            $('#v_columns').val(8);
        } else {
            $('#v_columns').val(16);
        }
        $("input[name='viewStyle'][value='Channel']").trigger('click');
        clearStream();

        // Trigger updated elements
        $('#s_proto').trigger('click');
        $('#s_count').trigger('change');
    }

    // PWM Config
    if (config.device.mode & 0x04) {  // PWM
        $('#o_pwm').removeClass('hidden');
        $('#pwm_enabled').prop('checked', config.pwm.enabled);
        if (config.pwm.enabled) {
            $('.pwm').removeClass('hidden');
        } else {
            $('.pwm').addClass('hidden');
        }

        $('#pwm_freq').val(config.pwm.freq);
        $('#pwm_gamma').prop('checked', config.pwm.gamma);

        addPWMTable( '#gpio_start' );

        var after = '#pwmtbody';
        for(var i=0, len=gpio_list.length; i < len; i++){
            var gpio_num = gpio_list[i];
            var gpioN = 'gpio' + gpio_num;

            addPWMTableRow( after, gpio_list[i] )
            if (typeof config['pwm']['gpio'][gpio_num] === 'undefined') {
                $('#' + gpioN +'_enabled').attr('disabled', 'true');
                $('#' + gpioN +'_outputType').attr('disabled', 'true');
                $('#' + gpioN +'_channel').val('-');
                $('#' + gpioN +'_channel').attr('disabled', 'true');
                $('#' + gpioN +'_comment').val('-');
                $('#' + gpioN +'_comment').attr('disabled', 'true');
            } else {
                if (typeof config['pwm']['gpio'][gpio_num]['enabled'] === 'undefined') {
                    $('#' + gpioN +'_enabled').prop('checked', false);
                    $('#' + gpioN +'_enabled').attr('disabled', 'true');
                } else {
                    $('#' + gpioN +'_enabled').prop('checked', config['pwm']['gpio'][gpio_num]['enabled']);
                }

                var outputType = 0;
                if (config['pwm']['gpio'][gpio_num]['invert'])
                    outputType += 1;
                if (config['pwm']['gpio'][gpio_num]['digital'])
                    outputType += 2;
                $('#' + gpioN +'_outputType').val(outputType);

                $('#' + gpioN +'_channel').val(config['pwm']['gpio'][gpio_num]['channel']);
                $('#' + gpioN +'_comment').val(config['pwm']['gpio'][gpio_num]['comment']);
            }
            after = '#' + gpioN;
        }
    }
}

function addPWMTable( afterID) {
    var newitem = "";
    newitem += '<table border="1" class="table esps-table" id="pwmtable">'
    newitem += '    <colgroup>'
    newitem += '        <col class="col-sm-offset-2 col-sm-1">'
    newitem += '        <col class="col-sm-1">'
    newitem += '        <col class="col-sm-1">'
    newitem += '        <col class="col-sm-1">'
    newitem += '        <col class="col-sm-5">'
    newitem += '    </colgroup>'
    newitem += '<thead>'
    newitem += '<tr>'
    newitem += '<th class="text-center" sope="col">GPIO</th>'
    newitem += '<th class="text-center" sope="col">Enabled</th>'
    newitem += '<th class="text-center" sope="col">Output Type</th>'
    newitem += '<th class="text-center" sope="col">DMX</th>'
    newitem += '<th class="text-center" sope="col">Description</th>'
    newitem += '</tr>'
    newitem += '</thead>'

    newitem += '<tbody id="pwmtbody">'
    newitem += '</tbody>'
    newitem += '</table>'

    $( '#pwmtable').remove();
    $( afterID ).append( newitem );
}

function addPWMTableRow( afterID, gpio ) {
var newitem = "";
newitem += '<tr id="gpio' + gpio + '">'
newitem += '  <td class="text-center">' + gpio + '</td>'
newitem += '  <td class="text-center"><input type="checkbox" id="gpio' + gpio + '_enabled" name="gpio' + gpio + '_enabled"></td>'

// Output Type
newitem += '  <td class="text-center"><select id="gpio' + gpio + '_outputType" name="gpio' + gpio + '_outputType">'
newitem += '  <option value="0">Normal</option>'
newitem += '  <option value="1">Inverted</option>'
newitem += '  <option value="2">Digital</option>'
newitem += '  <option value="3">Dig+Inv</option>'
newitem += '  </select></td>'

newitem += '  <td><input type="text" class="form-control" id="gpio' + gpio + '_channel" name="gpio' + gpio + '_channel" placeholder="DMX Channel"></td>'
newitem += '  <td><input type="text" class="form-control" id="gpio' + gpio + '_comment" name="gpio' + gpio + '_comment" placeholder="Description"></td>'
newitem += '</tr>'

//$( '#gpio'+gpio ).remove();
$( afterID ).after( newitem );
}


function getConfigStatus(data) {
    var status = JSON.parse(data);

    $('#x_ssid').text(status.ssid);
    $('#x_hostname').text(status.hostname);
    $('#x_ip').text(status.ip);
    $('#x_mac').text(status.mac);
    $('#x_version').text(status.version);
    $('#x_built').text(status.built);
    $('#x_flashchipid').text(status.flashchipid);
    $('#x_usedflashsize').text(status.usedflashsize);
    $('#x_realflashsize').text(status.realflashsize);
    $('#x_freeheap').text(status.freeheap);
}

function getEffectInfo(data) {
    parsed = JSON.parse(data);

    effectInfo = parsed.effectList;	// global effectInfo
    var running = parsed.currentEffect;

//  console.log (effectInfo);
//  console.log (effectInfo.t_chase);

    // process the effect configuration options
    $('#tmode').empty(); // clear the dropdown first
    for (var i in effectInfo) {
        var htmlid = effectInfo[i].htmlid;
        var name =   effectInfo[i].name;
        $('#tmode').append('<option value="' + htmlid + '">' + name + '</option>');
        if ( ! name.localeCompare(running.name) ) {
            $('#tmode').val(htmlid);
            hideShowTestSections();
        }
    }

    // set html based on current running effect
    $('.color').val('rgb(' + running.r + ',' + running.g + ',' + running.b + ')');
    $('.color').css('background-color', 'rgb(' + running.r + ',' + running.g + ',' + running.b + ')');
    $('#t_reverse').prop('checked', running.reverse);
    $('#t_mirror').prop('checked', running.mirror);
    $('#t_allleds').prop('checked', running.allleds);
    $('#t_startenabled').prop('checked', running.startenabled);
    $('#t_idleenabled').prop('checked', running.idleenabled);
    $('#t_idletimeout').val(running.idletimeout);
    $('#t_sendprotocol').val(running.sendprotocol);
    $('#t_sendhost').val(running.sendhost);
    $('#t_sendport').val(running.sendport);
    $('#t_sendspeed').val(running.sendspeed);
}

function getJsonStatus(data) {
    var status = JSON.parse(data);

    var rssi = +status.system.rssi;
    var quality = 2 * (rssi + 100);

    if (rssi <= -100)
        quality = 0;
    else if (rssi >= -50)
        quality = 100;

    $('#x_rssi').text(rssi);
    $('#x_quality').text(quality);

// getHeap(data)
    $('#x_freeheap').text( status.system.freeheap );

// getUptime
    $('#x_uptime').text( millsToDateString(+status.system.uptime, "") );

// data source
    $('#x_datasource').text( status.system.datasource );
    $('#x_effectname').text( status.system.effectname );

// getE131Status(data)
    $('#uni_first').text(status.e131.universe);
    $('#uni_last').text(status.e131.uniLast);
    $('#pkts').text(status.e131.num_packets);
    $('#serr').text(status.e131.seq_errors);
    $('#perr').text(status.e131.packet_errors);
    $('#clientip').text(status.e131.last_clientIP);
    $('#e131_lastseen').text(status.e131.last_seen);
    $('#e131_lastseen').text( millsToDateString(status.e131.last_seen, "Never") );

// getMQTTStatus(data)
    $('#mqtt_pkts').text(status.mqtt.num_packets);
    $('#mqtt_lastseen').text( millsToDateString(status.mqtt.last_seen, "Never") );

// getUDPStatus(data)
    $('#udp_pkts').text(status.udp.num_packets);
    $('#udp_shortpkts').text(status.udp.short_packets);
    $('#udp_longpkts').text(status.udp.long_packets);
    $('#udp_clientip').text(status.udp.last_clientIP);
    $('#udp_lastseen').text( millsToDateString(status.udp.last_seen, "Never") );
}

function refreshGamma(data) {
    var gammaData = JSON.parse(data);

    var polyline = document.getElementById('cracker');
    var points = polyline.getAttribute('points');

    points = "";
    for (X=0; X<256; X++) {
	var Y = 255-gammaData.gamma[X];
	points += X + ", "+ Y +" ";
    }

    polyline.setAttribute('points', points);
}

function snackSave() {
    // Show snackbar for 3sec
    var x = document.getElementById('snackbar');
    x.className = 'show';
    setTimeout(function(){
        x.className = x.className.replace('show', '');
    }, 3000);
}

function setConfig() {
    // Get config to refresh UI and show result
    wsEnqueue("G1");
    snackSave();
}

function submitWiFi() {
    var ip = $('#ip').val().split('.');
    var netmask = $('#netmask').val().split('.');
    var gateway = $('#gateway').val().split('.');

    var json = {
            'network': {
                'ssid': $('#ssid').val(),
                'passphrase': $('#password').val(),
                'hostname': $('#hostname').val(),
                'ip': [parseInt(ip[0]), parseInt(ip[1]), parseInt(ip[2]), parseInt(ip[3])],
                'netmask': [parseInt(netmask[0]), parseInt(netmask[1]), parseInt(netmask[2]), parseInt(netmask[3])],
                'gateway': [parseInt(gateway[0]), parseInt(gateway[1]), parseInt(gateway[2]), parseInt(gateway[3])],
                'dhcp': $('#dhcp').prop('checked'),
                'ap_fallback': $('#ap').prop('checked'),
                'udp_enabled': $('#udp_enabled').prop('checked'),
                'udp_port': $('#udp_port').val()
            }
        };
    wsEnqueue('S1' + JSON.stringify(json));
}

function submitConfig() {
    var channels = parseInt($('#s_count').val());
    if (mode == 'pixel')
        channels = parseInt($('#p_count').val()) * 3;

    var json = {
            'device': {
                'id': $('#devid').val()
            },
            'mqtt': {
                'enabled': $('#mqtt').prop('checked'),
                'ip': $('#mqtt_ip').val(),
                'port': $('#mqtt_port').val(),
                'user': $('#mqtt_user').val(),
                'password': $('#mqtt_password').val(),
                'topic': $('#mqtt_topic').val(),
                'clean': $('#mqtt_clean').prop('checked')
            },
            'e131': {
                'universe': parseInt($('#universe').val()),
                'universe_limit': parseInt($('#universe_limit').val()),
                'channel_start': parseInt($('#channel_start').val()),
                'channel_count': channels,
                'multicast': $('#multicast').prop('checked')
            },
            'pixel': {
                'type': parseInt($('#p_type').val()),
                'color': parseInt($('#p_color').val()),
                'gamma': $('#p_gamma').prop('checked'),
                'gammaVal': parseFloat($('#p_gammaVal').val()),
                'briteVal': parseFloat($('#p_briteVal').val())
            },
            'serial': {
                'type': parseInt($('#s_proto').val()),
                'baudrate': parseInt($('#s_baud').val())
            },
            "pwm": {
               "enabled": $('#pwm_enabled').prop('checked'),
               "freq": parseInt($('#pwm_freq').val()),
               "gamma": $('#pwm_gamma').prop('checked'),
               "gpio": {}
            }
        };

    for(var i = 0, len = gpio_list.length; i < len; i++) {
        var tg = gpio_list[i];

        json['pwm']['gpio'][tg] = {};
        json['pwm']['gpio'][tg]['channel'] = parseInt($('#gpio'+tg+'_channel').val());
        json['pwm']['gpio'][tg]['enabled'] = $('#gpio'+tg+'_enabled').prop('checked');

        json['pwm']['gpio'][tg]['invert'] = ($('#gpio'+tg+'_outputType option:selected').val() & 1)>0;
        json['pwm']['gpio'][tg]['digital'] = ($('#gpio'+tg+'_outputType option:selected').val() & 2)>0;

        json['pwm']['gpio'][tg]['comment'] = $('#gpio'+tg+'_comment').val();

    }

    wsEnqueue('S2' + JSON.stringify(json));
}

function submitStartupEffect() {
// not pretty - get current r,g,b from color picker
    var temp = $('.color').val().split(/\D+/);

    var currentEffectName = effectInfo[ $('#tmode option:selected').val() ].name;

    var json = {
            'effects': {
                'name': currentEffectName,
                'mirror': $('#t_mirror').prop('checked'),
                'allleds': $('#t_allleds').prop('checked'),
                'reverse': $('#t_reverse').prop('checked'),
                'r': temp[1],
                'g': temp[2],
                'b': temp[3],
                'brightness': 255,
                'startenabled': $('#t_startenabled').prop('checked'),
                'idleenabled': $('#t_idleenabled').prop('checked'),
                'idletimeout': parseInt($('#t_idletimeout').val()),
                'sendprotocol': $('#t_sendprotocol').val(),
                'sendhost': $('#t_sendhost').val(),
                'sendport': parseInt($('#t_sendport').val()),
                'sendspeed': parseFloat($('#t_sendspeed').val())
            }
        };

    wsEnqueue('S3' + JSON.stringify(json));
}

function refreshPixel() {
    var proto = $('#p_type option:selected').text();
    var size = parseInt($('#p_count').val());
    var frame = 30;
    var idle = 300;

    if (!proto.localeCompare('WS2811 800kHz')) {
        frame = 30;
        idle = 300;
    } else if (!proto.localeCompare('GE Color Effects')) {
        frame = 790;
        idle = 35;
    }

    var rate = (frame * size + idle) / 1000;
    var hz = 1000 / rate;
    $('#refresh').html(Math.ceil(rate) + 'ms / ' + Math.floor(hz) + 'Hz');
}

function refreshSerial() {
    var proto = $('#s_proto option:selected').text();
    var baud = parseInt($('#s_baud').val());
    var size = parseInt($('#s_count').val());
    var symbol = 11;
    if (!proto.localeCompare('Renard')) {
        symbol = 10;
        size = size + 2;
    } else if (!proto.localeCompare('DMX512')) {
        symbol = 11;
        baud = 250000;
        $('#s_baud').val(baud);
    }
    var rate = symbol * 1000 / baud * size;
    var hz = 1000 / rate;
    $('#refresh').html(Math.ceil(rate) + 'ms / ' + Math.floor(hz) + 'Hz');
}

function hideShowTestSections() {
    // Test mode toggles
//    $('.tdiv').addClass('hidden');
//    $('#'+$('select[name=tmode]').val()).removeClass('hidden');

    var tmode = $('#tmode option:selected').val();
//console.log('tmode is: ' + tmode);
    if ( (typeof tmode !== 'undefined') && (typeof effectInfo[tmode].wsTCode !== 'undefined') ) {
        if (effectInfo[tmode].hasColor) {
            $('#lab_color').removeClass('hidden');
            $('#div_color').removeClass('hidden');
	} else {
            $('#lab_color').addClass('hidden');
            $('#div_color').addClass('hidden');
        }
        if (effectInfo[tmode].hasMirror) {
            $('#div_mirror').removeClass('hidden');
	} else {
            $('#div_mirror').addClass('hidden');
        }
        if (effectInfo[tmode].hasReverse) {
            $('#div_reverse').removeClass('hidden');
	} else {
            $('#div_reverse').addClass('hidden');
        }
        if (effectInfo[tmode].hasAllLeds) {
            $('#div_allleds').removeClass('hidden');
	} else {
            $('#div_allleds').addClass('hidden');
        }
    }
}
function showCountPixels(){
    currentCount = $('#p_count').val();
    setCountPixelValue(currentCount, 0);
    $('#countpxl').modal();
    $('#pix_count').val(currentCount);
    if (currentCount === 1){
        $('#btn_pcminus1').disable = true;
    }
}
function changeCountPixel(value) {
    targetValue = Number($('#pix_count').val()) + value;
    targetValue = targetValue < 1? 1: targetValue;
    targetValue = targetValue > 680? 680: targetValue;
    $('#pix_count').val(targetValue);
    if (targetValue <= 5){
        $('#btn_pcminus5').attr('disabled', 'disabled')
    } else {
        $('#btn_pcminus5').removeAttr('disabled')
    }
    if (targetValue <= 1){
        $('#btn_pcminus1').attr('disabled', 'disabled')
    } else {
        $('#btn_pcminus1').removeAttr('disabled')
    }
    if (targetValue >= 680){
        $('#btn_pcplus1').attr('disabled', 'disabled')
    } else {
        $('#btn_pcplus1').removeAttr('disabled')
    }
    if (targetValue >= 676){
        $('#btn_pcplus5').attr('disabled', 'disabled')
    } else {
        $('#btn_pcplus5').removeAttr('disabled')
    }
    setCountPixelValue(targetValue, 1);
}
function setCountPixelValue(value, cmdtype) {
    var json = {
        'pixelcount': {
            'count': value,
            'commandtype': cmdtype
        }
    };
    wsEnqueue('S5' + JSON.stringify(json));
}
function setpixelcount() {
    $('#p_count').val($('#pix_count').val());
    closepixelcount();
}
function closepixelcount(){
    $('#countpxl').modal('hide');
    setCountPixelValue(0,2);
}
// effect selector changed
function effectChanged() {
    hideShowTestSections();

    var tmode = $('#tmode option:selected').val();

//console.log ('found tcode ' + effectInfo[tmode].wsTCode);
    if (typeof effectInfo[tmode].wsTCode !== 'undefined') {
        wsEnqueue( effectInfo[tmode].wsTCode );
    }
}

// effect send protocol changed
function sendChanged() {
//    hideShowTestSections();

    var protocol = $('#t_sendprotocol option:selected').val();

    // effects send UDP data toggle
    if (protocol > 0) {
        $('.sendeffect').removeClass('hidden');
    } else {
        $('.sendeffect').addClass('hidden');
    }

}

function showReboot() {
    $('#update').modal('hide');
    $('#reboot').modal();
    setTimeout(function() {
        if($('#dhcp').prop('checked')) {
            window.location.assign("/");
        } else {
            window.location.assign("http://" + $('#ip').val());
        }
    }, 5000);
}

function reboot() {
    showReboot();
    wsEnqueue('X6');
}


function sendGamma() {
    var json = {
        'pixel': {
            'gamma': $('#p_gamma').prop('checked'),
            'gammaVal': parseFloat($('#p_gammaVal').val()),
            'briteVal': parseFloat($('#p_briteVal').val())
        }
    }
    wsEnqueue('S4' + JSON.stringify(json));
    wsEnqueue('G4'); // Get Gamma Table
}

function millsToDateString(millis, stringIfZero) {

    if ( (millis > 0) || (stringIfZero.length == 0) ){
        var date = new Date(+millis);
        var str = '';
        str += Math.floor(date.getTime()/86400000) + " days, ";
        str += ("0" + date.getUTCHours()).slice(-2) + ":";
        str += ("0" + date.getUTCMinutes()).slice(-2) + ":";
        str += ("0" + date.getUTCSeconds()).slice(-2);
        return (str);
    } else {
        return (stringIfZero);
    }
}
