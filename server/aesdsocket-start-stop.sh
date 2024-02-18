#!/bin/sh

case "$1" in
  start)
      echo "Start Daemon"
      start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- '-d'
      ;;
  stop)
      echo "Stop Daemon"
      start-stop-daemon -K -n aesdsocket
      ;;
  *)
      echo "Usage: $0 {start|stop}"
      exit 1Daemon
esac
