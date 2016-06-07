#!/usr/bin/python

import sys, string, locale

def init_lists():
	#List of map block numbers for inode and block bitmaps
	block_map_nums = []
	inode_map_nums = []
	with open("group.csv", "r") as file_1:
		for line in file_1:
			group_line = line.rstrip('\n').split(',')
			block_map_nums.append(group_line[5])
			inode_map_nums.append(group_line[4])
	#List of free blocks and inodes
	free_blocks = []
	free_inodes = []
	with open("bitmap.csv", "r") as file_2:
		for line in file_2:
			bitmap_line = line.rstrip('\n').split(',')
			if(bitmap_line[0] in block_map_nums):
				free_blocks.append(int(bitmap_line[1]))
			if(bitmap_line[0] in inode_map_nums):
				free_inodes.append(int(bitmap_line[1]))
	#Important information from the super block, such as block size, inodes per group etc
	block_size = 0
	total_inodes = 0
	inodes_per_group = 0
	total_blocks = 0
	with open("super.csv", "r") as file_3:
		for line in file_3:
			super_line = line.rstrip('\n').split(',')
			total_blocks = int(super_line[2])
			block_size = int(super_line[3])
			total_inodes = int(super_line[1])
			inodes_per_group = int(super_line[6])
	#Return a tuple of lists to be able to index into to get the respective values
	return (free_blocks, free_inodes, block_map_nums, inode_map_nums, block_size, total_inodes, inodes_per_group, total_blocks)

#Function that finds invalid blocks and unallocated blocks and creates a dictionary of duplicates
def block_errors(out_file):
	#Create a duplicate dictionary that holds block number keys and list of referenced inode lists
	global duplicate_dict
	lists = init_lists()
	free_blocks = lists[0]
	block_size = lists[4]
	total_blocks = lists[7]
	duplicate_dict = {}
	with open("inode.csv", "r") as file_3:
		#Dictionary of block number keys to the corresponding inodes in the free list
		block_dict = {}
		for line in file_3:
			inode_line = line.rstrip('\n').split(',')
			#Get only the data block pointers
			inode_line_sub = inode_line[11:]
			#Counter for entry number in data block pointers
			counter = 0
			for data_block in inode_line_sub:
				#Converts hex to decimal
				dec_data_block = int(data_block, 16)
				#First 12 direct data block pointers
				if(counter <= 11):
					if(dec_data_block > int(total_blocks)):
						out_file.write("INVALID BLOCK < " + str(dec_data_block) + " > IN INODE < " + str(inode_line[0]) + " > ENTRY < " + str(counter) + " >\n")
					#Check for block in free blocks
					if(dec_data_block in free_blocks):
						#Initialize block dictionary
						if(str(dec_data_block) not in block_dict):
							block_dict[str(dec_data_block)] = []
						temp = block_dict[str(dec_data_block)]
						temp.append(" INODE < " + str(inode_line[0]) + " > ENTRY < " + str(counter) + " >")
						block_dict[str(dec_data_block)] = temp
					#Initialize the duplicate dictionary
					if(str(dec_data_block) not in duplicate_dict):
						duplicate_dict[str(dec_data_block)] = []
					temp = duplicate_dict[str(dec_data_block)]
					temp.append(" INODE < " + str(inode_line[0]) + " > ENTRY < " + str(counter) + " >")
					duplicate_dict[str(dec_data_block)] = temp
					counter += 1
				#Single indirect block pointer
				elif(counter == 12):
					if(dec_data_block > int(total_blocks)):
						out_file.write("INVALID BLOCK < " + str(dec_data_block) + " > IN INODE < " + str(inode_line[0]) + " > ENTRY < " + "12" + " >\n")
					if(dec_data_block != 0):
						with open("indirect.csv", "r") as file_4:
							for indirect in file_4:
								indirect_line = indirect.rstrip('\n').split(',')
								dec_indirect_block = int(indirect_line[2], 16)
								#Check for invalid block pointers
								if(dec_indirect_block == 0 or dec_indirect_block > int(total_blocks)):
									out_file.write("INVALID BLOCK < " + str(dec_indirect_block) + " > IN INODE < " + str(inode_line[0]) + " > INDIRECT BLOCK < " + str(int(inode_line_sub[12], 16)) + " > ENTRY < " + str(indirect_line[1]) + " >\n")
								#Check for pointed blocks in free block map, and add to the respective dictionaries
								if(dec_indirect_block in free_blocks):
									if(str(dec_indirect_block) not in block_dict):
										block_dict[str(dec_indirect_block)] = []
									temp = block_dict[str(dec_indirect_block)]
									temp.append(" INODE < " + str(inode_line[0]) + " > INDIRECT BLOCK < " + str(int(inode_line_sub[12], 16)) + " > ENTRY < " + str(indirect_line[1]) + " >")
									block_dict[str(dec_indirect_block)] = temp
								if(str(dec_indirect_block) not in duplicate_dict):
									duplicate_dict[str(dec_indirect_block)] = []
								temp = duplicate_dict[str(dec_indirect_block)]
								temp.append(" INODE < " + str(inode_line[0]) + " > INDIRECT BLOCK < " + str(int(inode_line_sub[12], 16)) + " > ENTRY < " + str(indirect_line[1]) + " >")
								duplicate_dict[str(dec_data_block)] = temp
								counter += 1
				#Double indirect block pointer
				elif(counter == 13):
					if(dec_data_block < 0 or dec_data_block > int(total_blocks)):
						out_file.write("INVALID BLOCK < " + str(dec_data_block) + " > IN INODE < " + str(inode_line[0]) + " > ENTRY < " + "13" + " >\n")
					if(dec_data_block != 0):
						indirect_file = open("indirect.csv", "r")
						indirect_data = readlines(indirect_file)
						entry_offset = 0;
						#Go through each single indirect block from the double indirect
						for indirect in indirect_data:
							indirect_line = indirect.rstrip('\n').split(',')
							entry_offset = int(indirect_line[1]) * (block_size/4 - 1);
							#Check for invalid block pointers
							if(dec_indirect_block == 0 or dec_indirect_block > int(total_blocks)):
								out_file.write("INVALID BLOCK < " + str(dec_indirect_block) + " > IN INODE < " + str(inode_line[0]) + " > INDIRECT BLOCK < " + str(int(inode_line_sub[13], 16)) + " > ENTRY < " + str(indirect_line[1]) + " >\n")
							#Go through all the data blocks pointed by each indirect block pointer entry pointed to by the doubly indirect block
							if(int(indirect_line[2], 16) != 0):
								for indirect_2 in indirect_data:
									indirect_line = indirect_2.rstrip('\n').split(',')
									dec_indirect_block = int(indirect_line[2], 16)
									#Check for invalid data blocks
									if(dec_indirect_block ==  0 or dec_indirect_block > int(total_blocks)):
										out_file.write("INVALID BLOCK < " + str(dec_indirect_block) + " > IN INODE < " + str(inode_line[0]) + " > INDIRECT BLOCK < " + str(int(inode_line_sub[13], 16) + entry_offset) + " > ENTRY < " + str(indirect_line[1]) + " >\n")
									#Check for blocks in the free block map, and add to the respective dictionaries
									if(dec_indirect_block in free_blocks):
										if(str(dec_indirect_block) not in block_dict):
											block_dict[str(dec_indirect_block)] = []
										temp = block_dict[str(dec_indirect_block)]
										temp.append(" INODE < " + str(inode_line[0]) + " > INDIRECT BLOCK < " + str(int(inode_line_sub[13], 16)) + " > ENTRY < " + str(int(indirect_line[1]) + entry_offset) + " >")
										block_dict[str(dec_indirect_block)] = temp
									if(str(dec_indirect_block) not in duplicate_dict):
										duplicate_dict[str(dec_indirect_block)] = []
									temp = duplicate_dict[str(dec_indirect_block)]
									temp.append(" INODE < " + str(inode_line[0]) + " > INDIRECT BLOCK < " + str(int(inode_line_sub[13], 16)) + " > ENTRY < " + str(int(indirect_line[1]) + entry_offset) + " >")
									duplicate_dict[str(dec_data_block)] = temp
									counter += 1
				#Triple indirect block pointers
				elif(counter == 14):
					if(dec_data_block < 0 or dec_data_block > int(total_blocks)):
						out_file.write("INVALID BLOCK < " + str(dec_data_block) + " > IN INODE < " + str(inode_line[0]) + " > ENTRY < " + "14" + " >\n")
					if(dec_data_block != 0):
						#Go through the triple indirect block to each of the pointed to double indirect blocks
						indirect_file = open("indirect.csv", "r")
						indirect_data = readlines(indirect_file)
						entry_offset = 0;
						for indirect in indirect_data:
							indirect_line = indirect.rstrip('\n').split(',')
							entry_offset = int(indirect_line[1]) * (block_size/4 - 1)
							#Check for invalid block pointers in the double indirect block pointers pointed to by the triple indirect block pointers
							if(dec_indirect_block == 0 or dec_indirect_block > int(total_blocks)):
								out_file.write("INVALID BLOCK < " + str(dec_indirect_block) + " > IN INODE < " + str(inode_line[0]) + " > INDIRECT BLOCK < " + str(int(inode_line_sub[14], 16)) + " > ENTRY < " + str(indirect_line[1]) + " >\n")
							if(int(indirect_line[2], 16) != 0):
								#Go through the double indirect block pointers to the single indirect blocks
								for indirect_2 in indirect_data:
									indirect_line = indirect_2.rstrip('\n').split(',')
									entry_offset += int(indirect_line[1]) * (block_size/4 - 1)
									#Check for invalid block pointers in the single indirect block pointers pointed to by the double indirect block pointers
									if(dec_indirect_block == 0 or dec_indirect_block > int(total_blocks)):
										out_file.write("INVALID BLOCK < " + str(dec_indirect_block) + " > IN INODE < " + str(inode_line[0]) + " > INDIRECT BLOCK < " + str(int(inode_line_sub[14], 16) + entry_offset) + " > ENTRY < " + str(indirect_line[1]) + " >\n")
									if(int(indirect_line[2], 16) != 0):
										#Go through the single indirect block pointers to the data blocks
										for indirect_3 in indirect_data:
											indirect_line = indirect_3.rstrip('\n').split(',')
											dec_indirect_block = int(indirect_line[2], 16)
											#Check for invalid blocks pointed to by the single indirect block pointers
											if(dec_indirect_block == 0 or dec_indirect_block > int(total_blocks)):
												out_file.write("INVALID BLOCK < " + str(dec_indirect_block) + " > IN INODE < " + str(inode_line[0]) + " > INDIRECT BLOCK < " + str(int(inode_line_sub[14], 16) + entry_offset) + " > ENTRY < " + str(indirect_line[1]) + " >\n")
											if(dec_indirect_block in free_blocks):
												if(str(dec_indirect_block) not in block_dict):
													block_dict[str(dec_indirect_block)] = []
												temp = block_dict[str(dec_indirect_block)]
												temp.append(" INODE < " + str(inode_line[0]) + " > INDIRECT BLOCK < " + str(int(inode_line_sub[14], 16)) + " > ENTRY < " + str(int(indirect_line[1]) + entry_offset) + " >")
												block_dict[str(dec_indirect_block)] = temp
											if(str(dec_indirect_block) not in duplicate_dict):
												duplicate_dict[str(dec_indirect_block)] = []
											temp = duplicate_dict[str(dec_indirect_block)]
											temp.append(" INODE < " + str(inode_line[0]) + " > INDIRECT BLOCK < " + str(int(inode_line_sub[14], 16)) + " > ENTRY < " + str(int(indirect_line[1]) + entry_offset) + " >")
											duplicate_dict[str(dec_data_block)] = temp
											counter += 1
		#Go through the block dictionary to find out what to print for unallocated blocks
		for element in block_dict:
			if(len(block_dict[element]) > 0):
				out_file.write("UNALLOCATED BLOCK < " + str(element) + " > REFERENCED BY")
				for out_string in block_dict[element]:
					out_file.write(out_string)
				out_file.write("\n")

#Use the duplicate dictionary to find the multiply allocated blocks
def duplicate_allocated_blocks(out_file):
	global duplicate_dict
	for key in duplicate_dict:
		#If the number of referenced inodes for the respective block number is more than 1, then we know its multiply allocated 
		if(len(duplicate_dict[key]) > 1 and key != "0"):
			statement = "MULTIPLY REFERENCED BLOCK < " + key + " > BY"
			for tup in duplicate_dict[key]:
				statement += tup
			statement += '\n'
			out_file.write(statement)

#Check for unallocated inodes
def unallocated_inode(directory_file, out_file):
	global inode_data
	file_list = open_files()
	lists = init_lists()
	free_inodes = lists[1]
	#Dictionary of inodes to the referenced directory entries
	unallocated_dict = {}
	#Holds a list of allocated inodes from the inode.csv file for use in other functions
	inode_data = []
	with open("inode.csv", "r") as inode_file:
		for line in inode_file:
			inode_line = line.split(',')
			inode_data.append(inode_line[0])
	with open("directory.csv", "r") as file_1:
		for line in file_1:
			#Ignore the naming of the file/directory as that could have unwanted characters
			directory_line = line.split(',')[0:5]
			if(len(directory_line) == 5 and (int(directory_line[4]) in free_inodes or directory_line[4] not in inode_data)):
				if(str(directory_line[4]) not in unallocated_dict):
					unallocated_dict[str(directory_line[4])] = []
				temp = unallocated_dict[str(directory_line[4])]
				temp.append(" DIRECTORY < " + directory_line[0] + " > ENTRY < " + directory_line[1] + " >")
				unallocated_dict[str(directory_line[4])] = temp
	#Go through the unallocated inodes and print out the respective information
	for element in unallocated_dict:
		if(len(unallocated_dict[element]) > 0):
			out_file.write("UNALLOCATED INODE < " + element + " > REFERENCED BY")
			for out_string in unallocated_dict[element]:
				out_file.write(out_string)
			out_file.write("\n")
#Find missing inodes by looking through the allocated inodes and free inodes, and the overall number of inodes
def missing_inode(inode_file, out_file):
	global inode_data
	file_list = open_files()
	lists = init_lists()
	free_inodes = lists[1]
	inode_block_num = lists[3]
	total_inodes = lists[5]
	inodes_per_group = lists[6]
	with open("inode.csv", "r") as file_1:
		for line in file_1:
			inode_line = line.split(',')[0:10]
			#Ignoring the first 10 reserved inodes, look to see if the allocated inode is missing from the free list
			if(inode_line[5] == "0" and inode_line[0] not in free_inodes and int(inode_line[0]) > 10):
				list_num = 0
				#Find out which free list it should belong to
				for i in range(len(inode_block_num)):
					list_num += int(inodes_per_group)
					if(list_num > int(inode_line[0])):
						list_num = int(inode_block_num[i])
						break
				out_file.write("MISSING INODE < " + inode_line[0] + " > SHOULD BE IN FREE LIST < " + str(list_num) + " >\n")
	#Go through all inodes from 11 to max inode number to see if they don't exist in the free list or allocated inodes
	for i in range(11, int(total_inodes)):
		if(i not in free_inodes and str(i) not in inode_data):
			list_num = 0
			for i in range(len(inode_block_num)):
				list_num += int(inodes_per_group)
				if(list_num > i):
					list_num = int(inode_block_num[i])
					break
			out_file.write("MISSING INODE < " + str(i) + " > SHOULD BE IN FREE LIST < " + str(list_num) + " >\n")

#Calculate the link counts by looking at the references and the actual number of links in inode.csv
def incorrect_link_count(inode_file, out_file):
  with open("inode.csv", "r") as file_1:
    for line in file_1:
      inode_line = line.split(',')
      #Reported link count
      link_count = inode_line[5]
      inode_num = inode_line[0]
      reference_count = 0
      with open("directory.csv", "r") as file_2:
      	#Calculate the actual number of links by looking through all the directories
        for line_2 in file_2:
          directory_line = line_2.split(',')[0:5]
          if(len(directory_line) == 5 and directory_line[4] == inode_num):
            reference_count += 1
      if(int(link_count) != int(reference_count)):
        out_file.write("LINKCOUNT < " + str(inode_num) + " > IS < " + str(link_count) + " > SHOULD BE < " + str(reference_count) + " >\n")
          
#Look at the . and .. to look for inconsistent directory entries
def incorrect_directory_entry(directory_file, out_file):
  data = ""
  dataFile = open("directory.csv", "r")
  data = dataFile.readlines()
  with open("directory.csv", "r") as file_1:
    for line in file_1:
      directory_line = line.split(',')
      if(len(directory_line) == 6):
      	#Check for the current directory against its own parent inode, they should be the same, else its an error
        if(directory_line[5] == '"."\n'):
          if(directory_line[0] != directory_line[4]):
            out_file.write("INCORRECT ENTRY IN < " + directory_line[0] + " > NAME < . > LINK TO < " + directory_line[4] + " > SHOULD BE < " + directory_line[0] + " >\n")
        elif(directory_line[5] == '".."\n'):
          for line_2 in data:
            directory_line_2 = line_2.split(',')
            if(len(directory_line_2) == 6):
            #Look through the referenced parent directory to see if it contains a child directory that is the referencee directory
              if(directory_line_2[4] == directory_line[0] and directory_line_2[0] != directory_line[4] and directory_line_2[5] != '"."\n' and directory_line_2[5] != '".."\n'):
                out_file.write("INCORRECT ENTRY IN < " + directory_line[0] + " > NAME < .. > LINK TO < " + directory_line[4] + " > SHOULD BE < " + directory_line_2[0] + " >\n")

#Open all the files initially and put them in a list for easy closing
def open_files():
  super_f = open("super.csv", "r")
  group_f = open("group.csv", "r")
  bitmap_f = open("bitmap.csv", "r")
  inode_f = open("inode.csv", "r")
  directory_f = open("directory.csv", "r")
  indirect_f = open("indirect.csv", "r")
  out_f = open("lab3b_check.txt", "w+")
  file_list = [super_f, group_f, bitmap_f, inode_f, directory_f, indirect_f, out_f]
  return file_list

#Close all files
def close_files():
  f_list = open_files()
  for file in f_list:
    file.close()

#Generate all the possible errors by parsing all the given files
def main():
  file_list = open_files()
  block_errors(file_list[6]) 
  duplicate_allocated_blocks(file_list[6])
  unallocated_inode(file_list[4], file_list[6])
  missing_inode(file_list[3], file_list[6])
  incorrect_link_count(file_list[3], file_list[6])
  incorrect_directory_entry(file_list[3], file_list[6])
  close_files()

if __name__ == "__main__":
  main()
