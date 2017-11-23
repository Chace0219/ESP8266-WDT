
### import packages
import requests
import json
import paramiko
import MySQLdb
import threading
import time
import logging
import os, sys, glob, re
import datetime as dt

logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s: %(message)s ', datefmt='%Y-%m-%d %H:%M:%S')

class Checker(threading.Thread):

    def __init__(self, ):
        threading.Thread.__init__(self)

        # Threading Event instance
        self.event = threading.Event()
        # interval
        self.interval = 900

        #
        self.lasttime = dt.datetime.now()

        ### Log file open
        self.path = '/home/rpb/'
        self.logfilename = self.path + self.lasttime.strftime("%Y-%m-%d") + ".txt"
        self.rootlogger = logging.getLogger()
        self.handler = logging.FileHandler(self.logfilename)
        self.handler.setFormatter(logging.Formatter('%(asctime)s %(levelname)s: %(message)s'))
        self.rootlogger.addHandler(self.handler)

        logging.info('monitor is started')

        try:
            #self.ssh = chilkat.CkSsh()
            self.db = MySQLdb.connect(host="localhost", user="root", passwd="blueflame", db="rpb")  # name of the database
            self.cur = self.db.cursor(MySQLdb.cursors.DictCursor)

        except:
            logging.error('DB connection error!, we have to confirm about db server ip and password')
            self.event.set()
            return


    '''
    def SSHinit(self):
        success = self.ssh.UnlockComponent("xK2SZ6.SS1_fgiXIoz4kg55")
        if (success != True):
            print(self.ssh.lastErrorText())
            logging.error('chilkat unlock failed')
            print("chilkat unlock failed.")
            return False
        return True
    '''

    def getWDTIP(self, serverIP):
        query = "SELECT * FROM status "
        query += "WHERE serverIP LIKE '"
        query += serverIP
        query += "'"

        localIP = ''
        try:
            self.cur.execute(query)
            rows = self.cur.fetchall()
            for row in rows:
                localIP = row['localIP']
        except:
            logging.error('db error in getting wdt address of server ' + serverIP)
        return localIP

    def checkSSH(self, serverip, uid, pwd):
        try:
            client = paramiko.SSHClient()
            client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            client.connect(serverip, username=uid, password=pwd)
            client.close()
        except paramiko.AuthenticationException:
            logging.warning('ssh connect password error, server ip ' + serverip)
        except:
            logging.error('ssh connect error, server ip ' + serverip)
            return False
        return True

    def SSHreboot(self, serverip, uid, pwd):
        try:
            client = paramiko.SSHClient()
            client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            client.connect(serverip, username=uid, password=pwd)
            stdin, stdout, stderr = client.exec_command('sudo hard-reboot > /dev/null 2>&1 &')
            client.close()
        except paramiko.ssh_exception.SSHException:
            logging.error('ssh connect error, server ip' + serverip)
            return -1
        except:
            logging.error('ssh unknown error, server ip' + serverip)
            return -1

        try:
            client = paramiko.SSHClient()
            client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            client.connect(serverip, username=uid, password=pwd)
            stdin, stdout, stderr = client.exec_command('sudo hard-reboot > /dev/null 2>&1 &')
            client.close()
        except paramiko.ssh_exception.SSHException:
            logging.error('ssh connect error, server ip ' + serverip)
            return -1
        except:
            logging.error('ssh unknown error, server ip' + serverip)
            return -1
        return 0

    def checkGPU(self):
        try:
            response = requests.get("http://omgllc.ethosdistro.com/?json=yes")
        except:
            logging.error('api response error!')
            return
        try:
            if (response.status_code == 200):
                gpudata = response.json()
                for item in gpudata['rigs']:
                    gpus = int(gpudata['rigs'][item]['gpus'])
                    miner_instance = int(gpudata['rigs'][item]['miner_instance'])
                    serverIP = gpudata['rigs'][item]['ip']
                    logging.info('gpus:' + str(gpus) + ', miner_instance:' + str(miner_instance) + ', serverIP:' + str(serverIP))
                    if (miner_instance < gpus):
                        logging.warning('reboot condition is detected, server ip:' + serverIP + ', gpus:' + str(
                            gpus) + ', miner_instance:' + str(miner_instance))

                        # reboot via ssh
                        if (self.SSHreboot(serverIP, 'ethos', 'omg1$*$001') != 0):
                            # getting wdt ip
                            wdtip = self.getWDTIP(serverIP)
                            if (wdtip != ''):
                                # manual reboot using wdt device
                                manualResponse = requests.get('http://' + wdtip + '/manual')
                                if (manualResponse.status_code == 200):
                                    if (manualResponse.text.find('successfully') != -1):
                                        logging.warning(
                                            'manual reboot success in wdtip(' + wdtip + ')' + ', server(' + serverIP + ')')
                                    else:
                                        logging.error(
                                            'manual reboot setting error in wdtip(' + wdtip + ')' + ', server(' + serverIP + ')')
                                else:
                                    logging.error(
                                        'manual reboot failed in wdtip(' + wdtip + ')' + ', server(' + serverIP + ')')
                    elif (self.checkSSH(serverIP, 'ethos', 'omg1$*$001') == False):
                        # getting wdt ip
                        wdtip = self.getWDTIP(serverIP)
                        if (wdtip != ''):
                            # manual reboot using wdt device
                            manualResponse = requests.get('http://' + wdtip + '/manual')
                            if (manualResponse.status_code == 200):
                                if (manualResponse.text.find('successfully') != -1):
                                    logging.warning(
                                        'manual reboot success in wdtip(' + wdtip + ')' + ', server(' + serverIP + ')')
                                else:
                                    logging.error(
                                        'manual reboot setting error in wdtip(' + wdtip + ')' + ', server(' + serverIP + ')')
                            else:
                                logging.error(
                                    'manual reboot failed in wdtip(' + wdtip + ')' + ', server(' + serverIP + ')')
                return True
        except:
            pass
        return False

    def run(self):
        try:
            logging.info('********************* checking is started *******************************')
            self.checkGPU()
            logging.info('*************************************************************************')

            ''' # it will be used when startup script
            while not self.event.isSet():
                if ((dt.datetime.now() - self.lasttime).seconds >= self.interval):
                    # New log file Create
                    time_now = dt.datetime.now()
                    newLoggingName = self.path + time_now.strftime("%Y-%m-%d") + ".txt"

                    if (newLoggingName != self.logfilename):
                        self.handler.close()
                        self.rootlogger.removeHandler(self.handler)
                        self.logfilename = newLoggingName
                        self.handler = logging.FileHandler(self.logfilename)
                        self.handler.setFormatter(logging.Formatter('%(asctime)s %(levelname)s: %(message)s'))
                        self.rootlogger.addHandler(self.handler)
                    logging.info('********************* checking is started *******************************')
                    self.checkGPU()
                    logging.info('*************************************************************************')
                    self.lasttime = dt.datetime.now()
                time.sleep(1)
            '''

            self.event.set()

        except:
            logging.error('serious error is happened')
            self.event.set();

if __name__ == '__main__':
    try:
        checkerThread = Checker()
        checkerThread.start()
        while(True):
            if(checkerThread.event.is_set()):
                break
            time.sleep(1)

    except KeyboardInterrupt: # If CTRL+C is pressed, exit cleanly:
        checkerThread.event.set()
        logging.info('keyboard interrupt is happened')
        time.sleep(2)

    finally:
        logging.info('finished program')

