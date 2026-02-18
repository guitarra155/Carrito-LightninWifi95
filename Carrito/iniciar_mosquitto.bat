@echo off
echo Cerrando instancias previas de Mosquitto...
taskkill /F /IM mosquitto.exe >nul 2>&1

echo Iniciando Mosquitto con configuracion local...
"C:\Program Files\mosquitto\mosquitto.exe" -c local_mosquitto.conf -v
pause