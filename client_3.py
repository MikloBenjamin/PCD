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
image_semaphore = threading.Semaphore()
mutex = threading.Lock()
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

#  help for command
def commandDoc():
    parser = argparse.ArgumentParser(description='With this command you send information to the server about your image processing.')
    parser.add_argument('-fi', type=str, help='a path for a Folder with Image', required=True)
    parser.add_argument('-fpi', type=str, help='a path for a Folder with Processed Image', required=True)
    parser.add_argument('-n', dest='negativ', action='store_true', help='NEGATIV - image will be processing in negativ.')
    parser.add_argument('-s', dest='selia', action='store_true', help='SELIA- image processing option')
    parser.add_argument('-b', dest='blurr', action='store_true', help='BLURR - image processing option')
    parser.add_argument('-w', dest='black_white', action='store_true', help='BLACK & WHITE - image processing option')

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
        print(f"Directory for processed images '{path}' is created.")

def waitForConfirmation():
    global mutex
    print("MUTEX locked!")
    mutex.acquire()
    # mutex = False
    while mutex.locked():
    # while mutex == False:
        pass

def sendImages():
    global mutex
    global confirmation_semaphore
    global image_semaphore
    createFolder(pimage_folder_path)

    image_semaphore.acquire()
    name_counter = 0
    for img in list_of_images:
        image_name = list_of_image_names[name_counter]
        image = None
        try:
            image = open(img, "rb")
        except FileNotFoundError as error:
            print(error)
            continue

        # prepare second pachet with number of image parts
        image.seek(0, io.SEEK_END)
        nr_of_packets =  math.ceil(image.tell() / PACKET_SIZE)
        image.seek(0, io.SEEK_SET)
        nr_pack_in_bytes1, nr_pack_in_bytes2 = convertInBytes(nr_of_packets)

        print("Sending image: ", img)

        # 2. send number of packets
        # server MUST send a confirm msg (2.1)

        # print(f"sendImages: Prepare image '{image_name} for sending'...")
        s.send(bytearray([NUMBER_OF_PACKETS, nr_pack_in_bytes1, nr_pack_in_bytes2])) 
        
        # 2.1
        # print("sendImages: Number of packets sent : ", nr_of_packets, "\n\tneed confirmation!")
        # waitForConfirmation()
        confirmation_semaphore.acquire()
        # print("SZIVEM 1 Confirmed!")
        # print("sendImages: NUMBER OF PACKETS IS CONFIRMED.")
        packet_number = 0
        while packet_number < nr_of_packets:
            image_bytes_read = image.read(PACKET_SIZE)
            image_packet_size = len(image_bytes_read)
            packet_size_in_bytes1, packet_size_in_bytes2 = convertInBytes(image_packet_size)
            
            # 3. send image packets/parts
            # server MUST send confirm message (3.1)

            s.send(bytearray([PACKET, packet_size_in_bytes1, packet_size_in_bytes2]) + image_bytes_read)
            
            # 3.1
            if packet_number + 1 < nr_of_packets:
                # print("\n\nsendImages: SENDING THE IMAGE!!!\nimage packet sent! packet number = ", packet_number, "\n\tneed confirmation!")
                # waitForConfirmation()
                confirmation_semaphore.acquire()
                # print("SZIVEM 2 Confirmed!")
                # print("sendImages: image packet confirmed!")

            packet_number += 1
        image.close()
        print("### WAITING BEFORE SENDING NEXT IMAGE ###")
        image_semaphore.acquire()
        print("### CAN SEND NEXT IMEAGE ###")
        time.sleep(1)
        

def recieveImages():
    global mutex
    global confirmation_semaphore
    global image_semaphore
    # Waiting for server to send the processed image
    # recieving number of packets from server
    number_of_packets_from_server = 0
    name_counter = 0
    new_file = None
    while True:
        packet_from_server = s.recv(BUFF_MAX_SIZE)
        if packet_from_server[0] == CONFIRMATION:
            confirmation_semaphore.release()
        elif packet_from_server[0] == NUMBER_OF_PACKETS:
            print(f"recieveImages: I recieved packet with type {NUMBER_OF_PACKETS}")
            number_of_packets_from_server = getNumberOfImagePackets(packet_from_server) + 1
            new_file = open(f"{pimage_folder_path}/{list_of_image_names[name_counter]}", "wb")
            name_counter += 1   
        elif packet_from_server[0] == PACKET:
            # print(f"recieveImages: I recieved packet with type {PACKET}")
            new_file.write(packet_from_server[PACKET:])
        if number_of_packets_from_server > 0:
            number_of_packets_from_server -= 1
            s.send(bytearray([CONFIRMATION]))
        if number_of_packets_from_server == 0:
            if new_file is not None:
                print(f"recieveImages: Image '{list_of_image_names[name_counter - 1]}' SAVED!\n")
                new_file.close()
                new_file = None
                image_semaphore.release()
                if name_counter == len(list_of_image_names):
                    break
    print(f"recieveImages: DONE! Check the processed images in {pimage_folder_path}/!")


if __name__ == "__main__":
    # mutex = threading.Lock()
    
    arguments = commandDoc()
    correct, image_folder_path, pimage_folder_path, option = commandAnalyze(arguments)
    if correct == 1:
        print("Client_3: Expect exact 3 arguments.")
    elif correct == 2:
        print("Client_3: Wrong image processing option.")
    elif correct == 3:
        print("Client_3: Need image folder path.")
    else:
        s.connect((HOST, PORT))

        # prepare first pachet with nr_images and process option
        list_of_image_names = [name for name in os.listdir(image_folder_path) if name.endswith(".png")]
        list_of_images = glob.glob(f"{image_folder_path}/*.png")
        nr_of_image = len(list_of_images)
        nr_image_bytes1, nr_image_bytes2 = convertInBytes(nr_of_image)
        option_bytes1, option_bytes2 = convertInBytes(option)

        # 1: send number images and option
        #server DO NOT send a confirm msg

        buffer = bytearray([NR_IMAGES_AND_OP, nr_image_bytes1, nr_image_bytes2, option_bytes1, option_bytes2])
        s.send(buffer) 
        
        thread_send_images = threading.Thread(target=sendImages)
        thread_recieve_processed_images = threading.Thread(target=recieveImages)

        thread_send_images.start()
        thread_recieve_processed_images.start()

        thread_send_images.join()
        thread_recieve_processed_images.join()

        s.close()
               

