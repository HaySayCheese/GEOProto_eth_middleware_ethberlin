﻿cmake_minimum_required(VERSION 3.6)

set(SOURCE_FILES
    base/RequestMessage.h
    base/RequestMessage.cpp

    base/ResponseMessage.h
    base/ResponseMessage.cpp

    base/RequestCycleMessage.h
    base/RequestCycleMessage.cpp

    base/ResponseCycleMessage.h
    base/ResponseCycleMessage.cpp

    base/RequestMessageWithReservations.h
    base/RequestMessageWithReservations.cpp

    ReceiverInitPaymentRequestMessage.h
    ReceiverInitPaymentRequestMessage.cpp

    ReceiverInitPaymentResponseMessage.h
    ReceiverInitPaymentResponseMessage.cpp

    CoordinatorReservationRequestMessage.h
    CoordinatorReservationRequestMessage.cpp

    CoordinatorReservationResponseMessage.h
    CoordinatorReservationResponseMessage.cpp

    IntermediateNodeReservationRequestMessage.h
    IntermediateNodeReservationRequestMessage.cpp

    IntermediateNodeReservationResponseMessage.h
    IntermediateNodeReservationResponseMessage.cpp

    CoordinatorCycleReservationRequestMessage.h
    CoordinatorCycleReservationRequestMessage.cpp

    CoordinatorCycleReservationResponseMessage.h
    CoordinatorCycleReservationResponseMessage.cpp

    IntermediateNodeCycleReservationRequestMessage.h
    IntermediateNodeCycleReservationRequestMessage.cpp

    IntermediateNodeCycleReservationResponseMessage.h
    IntermediateNodeCycleReservationResponseMessage.cpp

    ParticipantsVotesMessage.cpp
    ParticipantsVotesMessage.h

    VotesStatusRequestMessage.hpp

    FinalPathConfigurationMessage.cpp
    FinalPathConfigurationMessage.h

    FinalPathCycleConfigurationMessage.h
    FinalPathCycleConfigurationMessage.cpp

    TTLProlongationRequestMessage.h
    TTLProlongationRequestMessage.cpp

    FinalAmountsConfigurationMessage.h
    FinalAmountsConfigurationMessage.cpp

    FinalAmountsConfigurationResponseMessage.h
    FinalAmountsConfigurationResponseMessage.cpp

    TTLProlongationResponseMessage.h
    TTLProlongationResponseMessage.cpp

    ParticipantsPublicKeysMessage.h
    ParticipantsPublicKeysMessage.cpp

    ParticipantVoteMessage.h
    ParticipantVoteMessage.cpp

    TransactionPublicKeyHashMessage.h
    TransactionPublicKeyHashMessage.cpp EthereumOutgoingReceiptMessage.cpp EthereumOutgoingReceiptMessage.h)

add_library(messages__payments ${SOURCE_FILES})
target_link_libraries(messages__payments

    messages__base__transaction

    common
    exceptions)
