const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE HTML> 
<meta name="viewport" content="width=device-width, initial-scale=1">
<html>
<head>
<style>
form {margin: 0 auto; width: 0 auto;padding: 1em; border: 1px solid #CCC;border-radius: 1em;display: table;}
div { display: table-row;  }
label { display: table-cell; text-align: right; vertical-align: middle;}
input { display: table-cell; margin: 10px 5px; vertical-align: middle; }
input[type=submit] {background-color: #4CAF50;color: white;padding: 12px 20px;border: none;border-radius: 4px;cursor: pointer;}
input[type=checkbox] { display: table-cell; margin: 10px 5px; vertical-align: middle; }
input[type=checkbox].toggle {
  -webkit-appearance: none;
  appearance: none;
  width: 62px;
  height: 28px;
  display: inline-block;
  position: relative;
  border-radius: 50px;
  overflow: hidden;
  outline: none;
  border: none;
  cursor: pointer;
  background-color: #707070;
  transition: background-color ease 0.3s;
}

 input[type=checkbox].toggle:before {
  content: "on off";
  display: block;
  position: absolute;
  z-index: 2;
  width: 24px;
  height: 24px;
  background: #fff;
  left: 2px;
  top: 2px;
  border-radius: 50%;
  font: 10px/28px Helvetica;
  text-transform: uppercase;
  font-weight: bold;
  text-indent: -22px;
  word-spacing: 37px;
  color: #fff;
  text-shadow: -1px -1px rgba(0,0,0,0.15);
  white-space: nowrap;
  box-shadow: 0 1px 2px rgba(0,0,0,0.2);
  transition: all cubic-bezier(0.3, 1.5, 0.7, 1) 0.3s;
}

 input[type=checkbox].toggle:checked {
  background-color: #4CD964;
}

 input[type=checkbox].toggle:checked:before {
  left: 32px;
}
  
</style>
</head>
<h1 align="center" style="color: #4485b8;">Suresh's<br>Weather Station</h1>
<p align="center" > Enter values below to connect to internet and press save. <br /><br /></p>
<body>
<form action="/" method="post">
<div>
<label for="ssid">WiFi SSID </label>
<input type="text" id="ssid"  name="ssid"  required>
</div>
<div>
<label for="password"> WiFi password </label>
<input type="password" id="password" name="password"  required>
</div>
<div>
<label for="locationl"> City </label>
<input type="text" id="location" name="location"  required>
</div>
<div>
<label for="Country"> Country </label>
<input type="text" id="Country" name="country"  required>
</div>
 <div>
<label for="Needle"> Needle </label>
<input class="toggle" type="checkbox" id="Needle" name="needle" >
 </div>  
<div>
<label></label>
<input type="submit">
</div> 
</form>
</body>
</html>
)=====";  

