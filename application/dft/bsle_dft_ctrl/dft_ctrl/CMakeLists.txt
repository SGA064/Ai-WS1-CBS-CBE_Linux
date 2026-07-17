set(
    sle_comm_src_list
    ${CMAKE_CURRENT_SOURCE_DIR}/dft_ctrl.c
)
set(BTH_SLE_RAM_LIST "${BTH_SLE_RAM_LIST}" "${sle_comm_src_list}" CACHE INTERNAL "sle ram obj list" FORCE)
