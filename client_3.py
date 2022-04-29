from http.client import SWITCHING_PROTOCOLS
import io
import os
import argparse
import shutil
import socket
import math
from click import FileError
import glob

HOST = "127.0.0.1"
PORT = 7366
BUFF_MAX_SIZE = 1024

NR_IMAGES_AND_OP = 0
CONFIRMATION = 1
NUMBER_OF_PACKETS = 2
PACKET = 3
ERROR = 4

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

def confirmedMessage(c_message):
    return 1 == int.from_bytes(c_message, 'big')
 

if __name__ == "__main__":
    arguments = commandDoc()
    correct, image_folder_path, pimage_folder_path, option = commandAnalyze(arguments)
    if correct == 1:
        print("Client_3: Expect exact 3 arguments.")
    elif correct == 2:
        print("Client_3: Wrong image processing option.")
    elif correct == 3:
        print("Client_3: Need image folder path.")
    else:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
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


            # create folder for processed images
            try:
                os.mkdir(pimage_folder_path)
                print(f"Directory for processed images '{pimage_folder_path}' is created.")
            except OSError as error:
                shutil.rmtree(pimage_folder_path)
                os.mkdir(pimage_folder_path)
                print(f"Directory for processed images '{pimage_folder_path}' is created.")

            name_counter = 0
            for img in list_of_images:
                image_name = list_of_image_names[name_counter]
                try:
                    image = open(img, "rb")

                    # prepare second pachet with number of image parts
                    image.seek(0, io.SEEK_END)
                    nr_of_packets =  math.ceil(image.tell() / 1021)
                    nr_pack_in_bytes1, nr_pack_in_bytes2 = convertInBytes(nr_of_packets)

                    # 2. send number of packets
                    # server MUST send a confirm msg (2.1)

                    buffer = bytearray([NUMBER_OF_PACKETS, nr_pack_in_bytes1, nr_pack_in_bytes2])
                    print(f"Prepare image '{image_name} for sending'...")
                    s.send(buffer) 
                    
                    # 2.1
                    print("Number of packets sent : ", nr_of_packets)
                    conf_message = s.recv(BUFF_MAX_SIZE)
                    print("CONFIRM: Server recieved.")
                    if not confirmedMessage(conf_message):
                        print("The message is not confirmed...")
                        print(conf_message)
                        break
                    else:
                        image.seek(0, 0)
                        packet_number = 0
                        while packet_number < nr_of_packets:
                            image_bytes_read = image.read(1021)
                            image_packet_size = len(image_bytes_read)
                            packet_size_in_bytes1, packet_size_in_bytes2 = convertInBytes(image_packet_size)
                            
                            # 3. send image packets/parts
                            # server MUST send confirm message (3.1)

                            buffer = bytearray([PACKET, packet_size_in_bytes1, packet_size_in_bytes2]) + image_bytes_read
                            s.send(buffer)
                            
                            # 3.1
                            if packet_number + 1 < nr_of_packets:
                                conf_message = s.recv(BUFF_MAX_SIZE)
                                if not confirmedMessage(conf_message):
                                    print("The message is not confirmed...")
                                    break
 
                            packet_number += 1
                    image.close()
                except FileNotFoundError as error:
                    print("Open Image...\n ",error)

                # Waiting for server to send the processed image
                # recieving number of packets from server

                print("Waiting for server...")

                number_of_packets_from_server = getNumberOfImagePackets(s.recv(BUFF_MAX_SIZE))
                if number_of_packets_from_server == b'':
                    print("NO Number of packet sended from server!")
                    break
                else:
                    print(f"Recieved number of packets from server: {number_of_packets_from_server}")

                    # sending confirmation 
                    buffer = bytearray([CONFIRMATION])
                    s.send(buffer)

                    # create a new png file for every processed image
                    packet_number = 0
                    with open(f"{pimage_folder_path}/{image_name}", "wb") as new_file:
                        while packet_number < number_of_packets_from_server:

                            # client recieve packets from server with processed image
                            # verifying the recieved message exists
                            # write in the new png file
                            
                            packet_from_server = s.recv(BUFF_MAX_SIZE)
                            if packet_from_server == b'':
                                print("Packet is empty!!!")
                                break
                            else:
                                s.send(buffer)
                                new_file.write(packet_from_server[PACKET:])
                                
                            packet_number += 1
                        name_counter += 1
                    print(f"Image '{image_name}' SAVED!\n")
            print("DONE! Check the processed images in the folder!")

