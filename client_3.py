import io
import os
import argparse
import threading
import socket
import math
import glob
import time

HOST = "127.0.0.1"
PORT = 7366
BUFF_MAX_SIZE = 1024
PACKET_SIZE = 1021

NR_IMAGES_AND_OP = 0
CONFIRMATION = 1
NUMBER_OF_PACKETS = 2
PACKET = 3
ERROR = 4

confirmation_semaphore = threading.Semaphore()
image_semaphore = threading.Semaphore(0)
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

log_file = None
def log(text):
    log_file.write(text + "\n")

#  help for command
def commandDoc():
    parser = argparse.ArgumentParser(description='With this command you send information to the server about your image processing.')
    parser.add_argument('-fi', type=str, help='a path for a Folder with Image', required=True)
    parser.add_argument('-fpi', type=str, help='a path for a Folder with Processed Image', required=True)
    parser.add_argument('-n', dest='negativ', action='store_true', help='NEGATIV - image will be processing in negativ.')
    parser.add_argument('-s', dest='selia', action='store_true', help='SELIA- image processing option')
    parser.add_argument('-b', dest='blurr', action='store_true', help='BLURR - image processing option')
    parser.add_argument('-w', dest='black_white', action='store_true', help='BLACK & WHITE - image processing option')
    parser.add_argument('-outlog', type=str, help='a path for a LOG file', required=True)

    args = parser.parse_args()
    return args

# command verification
def commandAnalyze(argumente):
    options = []
    for arg in vars(argumente):
        value = getattr(argumente, arg)
        if value != False:
            if arg in ["fi"]:
                options.append(value)
            elif arg in ["fpi"]:
                options.append(value)
            elif arg in ["outlog"]:
                global log_file
                log_file = open(value, "w")
            else:
                options.append(arg)
    if len(options) !=  3:
        return 1, "", ""
    elif len(options) == 2:
        return 2, "", ""
    elif len(options) == 0:
        return 3, "", ""
    else:
        if options[2] == "negativ":
            op = 0
        elif options[2] == "selia":
            op = 1
        elif options[2] == "blurr":
            op = 2
        else:
            op = 3
        return 0, options[0], options[1], op

# get number of images and option from buffer
def getNumberOfImageAndOption(buffer):
    number_of_packets = 0xff & buffer[1]
    number_of_packets |= (0xff & buffer[2]) << 8
    option_bytes = 0xff & buffer[3]
    option_bytes |= (0xff & buffer[4]) << 8

    return number_of_packets, option_bytes

# get number of image packets
def getNumberOfImagePackets(buffer):
    number_of_packets = 0xff & buffer[1]
    number_of_packets |= (0xff & buffer[2]) << 8

    return number_of_packets

def convertInBytes(number):
    number_in_bytes1 = number & 0xff
    number_in_bytes2 = (number >> 8) & 0xff

    return number_in_bytes1, number_in_bytes2

def createFolder(path):
    if not os.path.exists(path):
        os.mkdir(path)
        log(f"Directory for processed images '{path}' is created.")

def sendImages():
    createFolder(pimage_folder_path)

    for img in list_of_images:
        image = None
        try:
            image = open(img, "rb")
        except FileNotFoundError as error:
            log(error)
            continue

        image.seek(0, io.SEEK_END)
        nr_of_packets =  math.ceil(image.tell() / PACKET_SIZE)
        image.seek(0, io.SEEK_SET)
        nr_pack_in_bytes1, nr_pack_in_bytes2 = convertInBytes(nr_of_packets)

        log(f"Sending image: {img}")

        s.send(bytearray([NUMBER_OF_PACKETS, nr_pack_in_bytes1, nr_pack_in_bytes2])) 
        
        confirmation_semaphore.acquire()
        packet_number = 0
        while packet_number < nr_of_packets:
            image_bytes_read = image.read(PACKET_SIZE)
            image_packet_size = len(image_bytes_read)
            packet_size_in_bytes1, packet_size_in_bytes2 = convertInBytes(image_packet_size)
            
            s.send(bytearray([PACKET, packet_size_in_bytes1, packet_size_in_bytes2]) + image_bytes_read)
            
            if packet_number + 1 < nr_of_packets:
                confirmation_semaphore.acquire()

            packet_number += 1
        image.close()
        log("### WAITING BEFORE SENDING NEXT IMAGE ###")
        image_semaphore.acquire()
        log("### CAN SEND NEXT IMEAGE ###")
        # time.sleep(1)
        

def recieveImages():
    global nr_of_image

    option_str = ["NEGATIV", "SEPIA", "BLURR", "BLACK&WHITE"]
    number_of_packets_from_server = -1
    name_counter = 0
    new_file = None
    while nr_of_image > 0:
        packet_from_server = s.recv(BUFF_MAX_SIZE)
        if len(packet_from_server) <= 0:
            break
        if packet_from_server[0] == CONFIRMATION:
            confirmation_semaphore.release()
            continue
        elif packet_from_server[0] == NUMBER_OF_PACKETS:
            log(f"recieveImages: I recieved packet with type {NUMBER_OF_PACKETS}")
            number_of_packets_from_server = getNumberOfImagePackets(packet_from_server) + 1
            createFolder(f"{pimage_folder_path}/{option_str[option]}")
            new_file = open(f"{pimage_folder_path}/{option_str[option]}/{list_of_image_names[name_counter]}", "wb")
            name_counter += 1   
        elif packet_from_server[0] == PACKET:
            # log(f"recieveImages: I recieved packet with type {PACKET}")
            new_file.write(packet_from_server[PACKET:])
        if number_of_packets_from_server > 0:
            number_of_packets_from_server -= 1
            s.send(bytearray([CONFIRMATION]))
        if number_of_packets_from_server == 0:
            if new_file is not None:
                number_of_packets_from_server = -1
                nr_of_image -= 1
                log(f"recieveImages: Image '{list_of_image_names[name_counter - 1]}' SAVED! Saved in: {pimage_folder_path}{str(option)}\n ")
                new_file.close()
                new_file = None
                image_semaphore.release()


if __name__ == "__main__":    
    arguments = commandDoc()
    correct, image_folder_path, pimage_folder_path, option = commandAnalyze(arguments)
    if correct == 1:
        log("Client_3: Expect exact 3 arguments.")
    elif correct == 2:
        log("Client_3: Wrong image processing option.")
    elif correct == 3:
        log("Client_3: Need image folder path.")
    else:
        s.connect((HOST, PORT))

        list_of_image_names = [name for name in os.listdir(image_folder_path) if name.endswith(".png")]
        list_of_images = glob.glob(f"{image_folder_path}/*.png")
        nr_of_image = len(list_of_images)
        nr_image_bytes1, nr_image_bytes2 = convertInBytes(nr_of_image)
        option_bytes1, option_bytes2 = convertInBytes(option)

        buffer = bytearray([NR_IMAGES_AND_OP, nr_image_bytes1, nr_image_bytes2, option_bytes1, option_bytes2])
        s.send(buffer) 
        
        thread_send_images = threading.Thread(target=sendImages)
        thread_recieve_processed_images = threading.Thread(target=recieveImages)

        thread_send_images.start()
        thread_recieve_processed_images.start()

        thread_send_images.join()
        thread_recieve_processed_images.join()

        s.close()
    log("End client")
               