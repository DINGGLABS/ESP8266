String HTML_head =
  "<html>\
     <head>\
       <meta charset='utf-8'>\
       <title>test</title>\
       <h2><u>WiFi-Button</u></h2>\
     </head>\
     <body>";

String HTML_setLEDsForm =
       "<h3>Set LEDs</h3>\
        <form action='/api/gpio/leds' method='post' >\
          <label for='red'>RED</label>\
          <input type='range' min='0' value='$red' max='255' step='5' name='red' data-outputs-to='#output4'>\
          <label id='output4'></label>\
          <br>\
          <label for='green'>GREEN</label>\
          <input type='range' min='0' value='$green' max='255' step='5' name='green' data-outputs-to='#output5'>\
          <label id='output5'></label>\
          <br>\
          <label for='blue'>BLUE</label>\
          <input type='range' min='0' value='$blue' max='255' step='5' name='blue' data-outputs-to='#output6'>\
          <label id='output6'></label>\
          <br>\
          <br>\
          <input type='submit' label='Save'>\
          <br>\
          <br>\
          <hr>\
        </form>";

String HTML_setSSIDForm =
       "<h3>Set SSID</h3>\
        <p style='color: red'>$invalid</p>\
        <form action='/api/config/ssid' method='post' >\
          <label for='ssid'>SSID</label>\
          <input type='text' name='ssid'>\
          <br>\
          <label for='ssid_pw'>Password</label>\
          <input type='text' name='ssid_pw'>\
          <br>\
          (>, <, \", ' and & are not allowed)\
          <br>\
          <br>\
          <input type='submit' label='Save'>\
          <br>\
          <br>\
          <hr>\
        </form>";

String HTML_setTimeForm = 
       "<h3>Set time</h3>\
        <form action='/api/time' method='post' >\
          Standard Time<input type='radio' $st1 value='STD' name='sumTime' checked=true>\
          Summer Time<input type='radio' $st2 value='SUM' name='sumTime'>\
          <br>\
          <select name='utc'>\
            <option value='14'>UTC+14</option> <option value='13'>UTC+13</option>\
            <option value='12.45'>UTC+12:45</option> <option value='12'>UTC+12</option>\
            <option value='11.30'>UTC+11:30</option> <option value='11'>UTC+11</option>\
            <option value='10.30'>UTC+10:30</option> <option value='10'>UTC+10</option>\
            <option value='9.30'>UTC+9:30</option> <option value='9'>UTC+9</option>\
            <option value='8.45'>UTC+8:45</option> <option value='8.30'>UTC+8:30</option>\
            <option value='8'>UTC+8</option> <option value='7'>UTC+7</option>\
            <option value='6.30'>UTC+6:30</option> <option value='6'>UTC+6</option>\
            <option value='5.45'>UTC+5:45</option> <option value='5.30'>UTC+5:30</option>\
            <option value='5'>UTC+5</option> <option value='4.30'>UTC+4:30</option>\
            <option value='4'>UTC+4</option> <option value='3.30'>UTC+3:30</option>\
            <option value='3'>UTC+3</option> <option value='2'>UTC+2</option>\
            <option value='1' selected>UTC+1</option> <option value='0'>UTC+0</option>\
            <option value='-1'>UTC-1</option> <option value='-2'>UTC-2</option>\
            <option value='-2.30'>UTC-2:30</option> <option value='-3'>UTC-3</option>\
            <option value='-3.30'>UTC-3:30</option> <option value='-4'>UTC-4</option>\
            <option value='-4.30'>UTC-4:30</option> <option value='-5'>UTC-5</option>\
            <option value='-6'>UTC-6</option> <option value='-7'>UTC-7</option>\
            <option value='-8'>UTC-8</option> <option value='-9'>UTC-9</option>\
            <option value='-9.30'>UTC-9:30</option> <option value='-10'>UTC-10</option>\
            <option value='-11'>UTC-11</option> <option value='-12'>UTC-12</option>\
          </select>\
          <br>\
          <br>\
          <input type='submit' label='Save'>\
          <br>\
          <br>\
          <hr>\
        </form>";
        
String HTML_uploadFirmwareForm =
       "<h2><u>Reprogram your device wireless</u></h2>\
        <form action='/api/upload/firmware' method='post' enctype='multipart/form-data'>\
          Upload a *.bin-file from your directory...\
          <br>\
          <br>\
          <input type='file' name='updateProgram'>\
          <input type='submit' value='Upload'>\
          <br>\
          <br>\
          <hr>\
        </form>";

String HTML_saveFileForm =
       "<h2><u>Save a file into the FS of the Device</u></h2>\
        <form action='/api/upload/path' method='get' >\
          Set upload path (default is \"/srv\")...\
          <br>\
          <br>\
          <input type='text' name='path'>\
          <input type='submit' value='Set path'>\
          <br>\
          <br>\
          <hr>\
        </form>\
        <form action='/api/upload/file' method='post' enctype='multipart/form-data'>\
          Upload a file from your directory...\
          <br>\
          <br>\
          <input type='file' name='saveFile'>\
          <input type='submit' value='Upload'>\
          <br>\
          <br>\
          <hr>\
        </form>";

String HTML_resetDeviceForm =
       "<h3>Reset device</h3>\
        (disconnects the device from the WLAN and resets its configurations)\
        <form action='/api/config/reset' method='get' >\
          <input type='submit' label='Save'>\
          <br>\
          <br>\
          <hr>\
        </form>";
        
String JS_functions =
"<script>\
var watch_outputs = function()\
{\
  var inputs = document.querySelectorAll('input[data-outputs-to]');\
  \
  for(var i=0; i<inputs.length; i++)\
  {\
    var input = inputs[i];\
    \
    input.addEventListener('change', function(e)\
    {\
      var input = e.target;\
      var output = document.querySelector(input.getAttribute('data-outputs-to'));\
      output.innerHTML = input.value;\
    });\
    input.dispatchEvent(new Event('change'));\
  };\
};\
watch_outputs();\
</script>";

String HTML_tail =
    "</body>\
   </html>";

